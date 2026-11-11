# YO_simplify Makefile
#
# 使い方 (usage):
#   make normal                          # 通常版 (AVX2, clang++)
#   make normal TARGET_CPU=ZEN2          # CPUを指定
#   make normal COMPILER=g++             # コンパイラを指定
#   make tournament                      # 大会用 (千日手検出を簡略化)
#   make pgo EVAL_DIR=/path/to/eval      # PGOビルド (評価関数ファイルが必要)
#   make clean

# === ビルドオプション (build options) ===

# ターゲットCPU (target cpu)
#
# --- Intel/AMD (x64)
#   AVX512VNNI : AVX-512 + VNNI (Cascade Lake以降)
#   AVX512     : AVX-512 (サーバー向けSkylake以降)
#   AVXVNNI    : AVX2 + AVX-VNNI (Alder Lake以降)
#   AVX2       : AVX2 (Haswell以降)
#   SSE42 / SSE41 / SSSE3 / SSE2
# --- AMD Ryzen
#   ZEN1 / ZEN2    : BMI2(PEXT)が遅いので使わない
#   ZEN3           : BMI2を使う
#   ZEN3AVX512VNNI
# --- ARM (64bit)
#   ARMV8          : 汎用ARMv8
#   ARMV8_DOTPROD  : ARMv8 + dot product
#   GRAVITON2      : AWS Graviton2
#   WINARM64       : Windows ARM64 (ARMV8と同等)
# --- macOS
#   APPLEAVX2 / APPLESSE42 : Intel Mac
#   APPLEM1                : Apple Silicon (M1以降)
TARGET_CPU = AVX2

# 使用するコンパイラ (compiler)
# ※ clangでコンパイルしたほうがgccより数%速い。
COMPILER = clang++

# デバッガーを使用するか (debugger)
DEBUG = OFF

# エンジンの表示名 ("usi"コマンドに対して出力される)
#ENGINE_NAME =

# makeするときにCPPFLAGS,LDFLAGSを追加で指定したいときはこれを用いる。
EXTRA_CPPFLAGS =
EXTRA_LDFLAGS  =

# "make pgo"でプロファイルを採るときに使う評価関数フォルダ
EVAL_DIR = eval

# === implementations ===

CPPFLAGS  = -std=c++17 -fno-exceptions -fno-rtti -Wextra -MMD -MP -fpermissive
CPPFLAGS += $(EXTRA_CPPFLAGS)
CPPFLAGS += -O3 -ffast-math
WCPPFLAGS =
LDFLAGS   = $(EXTRA_LDFLAGS)

# デバッガーを使わないなら、NDEBUGをdefineする。
ifeq ($(DEBUG),OFF)
	CPPFLAGS += -DNDEBUG
endif

ifneq (,$(findstring clang++,$(COMPILER)))
	# 関数の引数が関数本体で使われていないときの警告を抑制。
	CPPFLAGS += -Wno-unused-parameter

	# Windowsでstaticリンクを行う際に __cxa_guard_acquire __cxa_guard_release の生成を抑止
	WCPPFLAGS += -fno-threadsafe-statics

	# clang++で-flto指定時は、lldを使わないとMSYS2やUbuntu 24.04のclang-18でlinkに失敗する。
	# macOSではld64を用いるので指定してはならない。
	ifeq (,$(findstring APPLE,$(TARGET_CPU)))
		LDFLAGS += -fuse-ld=lld
	endif
else ifneq (,$(findstring g++,$(COMPILER)))
	# mingw g++ で AVX512 向けビルドを行おうとするとエラーになる問題の回避
	# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65782
	WCPPFLAGS += -fno-asynchronous-unwind-tables
endif

ifeq ($(OS),Windows_NT)
	CPPFLAGS += $(WCPPFLAGS)
	LDFLAGS  += -static -Wl,--stack,25000000
	TARGET    = YaneuraOu-by-gcc.exe
else
	TARGET    = YaneuraOu-by-gcc
endif

# リンク時最適化。CPPFLAGSとLDFLAGSの両方で指定する必要がある。
LTOFLAGS = -flto

# wstringを使うためにこのシンボル定義が必要。
CPPFLAGS += -DUNICODE

# stripの指示。(実行ファイルのサイズを小さく)
LDFLAGS += -Wl,-s
LDFLAGS += -lpthread

ifneq ($(ENGINE_NAME),)
	CPPFLAGS += -DENGINE_NAME_FROM_MAKEFILE=$(ENGINE_NAME)
endif

# --- CPU別のフラグ

CPPFLAGS += -DTARGET_CPU=\"$(TARGET_CPU)\"

ifeq ($(TARGET_CPU),ZEN1)
	CPPFLAGS += -DUSE_AVX2 -mbmi -mno-bmi2 -mavx2 -march=znver1
else ifeq ($(TARGET_CPU),ZEN2)
	CPPFLAGS += -DUSE_AVX2 -mbmi -mno-bmi2 -mavx2 -march=znver2
else ifeq ($(TARGET_CPU),ZEN3)
	CPPFLAGS += -DUSE_AVX2 -DUSE_BMI2 -mbmi -mbmi2 -mavx2 -march=znver3
else ifeq ($(TARGET_CPU),ZEN3AVX512VNNI)
	CPPFLAGS += -DUSE_AVX512 -DUSE_BMI2 -DUSE_VNNI -march=znver3 -mbmi -mbmi2 -mavx512f -mavx512bw -mavx512vl -mavx512dq -mavx512vnni -mprefer-vector-width=512
else ifeq ($(TARGET_CPU),AVX512VNNI)
	CPPFLAGS += -DUSE_AVX512 -DUSE_BMI2 -DUSE_VNNI -march=cascadelake
else ifeq ($(TARGET_CPU),AVXVNNI)
	CPPFLAGS += -DUSE_AVX2 -DUSE_BMI2 -DUSE_VNNI -DUSE_AVXVNNI -march=alderlake
else ifeq ($(TARGET_CPU),AVX512)
	CPPFLAGS += -DUSE_AVX512 -DUSE_BMI2 -march=skylake-avx512
else ifeq ($(TARGET_CPU),AVX2)
	CPPFLAGS += -DUSE_AVX2 -DUSE_BMI2 -mbmi -mbmi2 -mavx2 -march=corei7-avx
else ifeq ($(TARGET_CPU),SSE42)
	CPPFLAGS += -DUSE_SSE42 -msse4.2 -march=corei7
else ifeq ($(TARGET_CPU),SSE41)
	CPPFLAGS += -DUSE_SSE41 -msse4.1 -march=core2
else ifeq ($(TARGET_CPU),SSSE3)
	CPPFLAGS += -DUSE_SSSE3 -msse3 -march=core2
else ifeq ($(TARGET_CPU),SSE2)
	CPPFLAGS += -DUSE_SSE2 -msse2 -march=core2
else ifeq ($(TARGET_CPU),GRAVITON2)
	CPPFLAGS += -DUSE_NEON=8 -march=armv8.2-a+fp16+rcpc+dotprod+crypto
else ifeq ($(TARGET_CPU),WINARM64)
	CPPFLAGS += -DUSE_NEON=8
else ifeq ($(TARGET_CPU),ARMV8)
	CPPFLAGS += -DUSE_NEON=8
else ifeq ($(TARGET_CPU),ARMV8_DOTPROD)
	CPPFLAGS += -DUSE_NEON=8 -DUSE_NEON_DOTPROD -march=armv8.2-a+dotprod
else ifeq ($(TARGET_CPU),APPLEAVX2)
	CPPFLAGS += -DUSE_AVX2 -DUSE_BMI2 -target x86_64-apple-macos11 -mbmi -mbmi2 -mavx2 -mpopcnt
else ifeq ($(TARGET_CPU),APPLESSE42)
	CPPFLAGS += -DUSE_SSE42 -target x86_64-apple-macos11
else ifeq ($(TARGET_CPU),APPLEM1)
	CPPFLAGS += -DUSE_NEON -target arm64-apple-macos11
else
$(error Unknown TARGET_CPU '$(TARGET_CPU)')
endif

# --- ソースファイル

OBJDIR = ../obj

SOURCES = \
	main.cpp                                     \
	types.cpp                                    \
	bitboard.cpp                                 \
	misc.cpp                                     \
	movegen.cpp                                  \
	position.cpp                                 \
	usi.cpp                                      \
	usioption.cpp                                \
	thread.cpp                                   \
	tt.cpp                                       \
	movepick.cpp                                 \
	timeman.cpp                                  \
	memory.cpp                                   \
	engine.cpp                                   \
	search.cpp                                   \
	score.cpp                                    \
	benchmark.cpp                                \
	book/book.cpp                                \
	extra/bitop.cpp                              \
	extra/sfen_packer.cpp                        \
	mate/mate.cpp                                \
	mate/mate1ply_without_effect.cpp             \
	eval/evaluate_bona_piece.cpp                 \
	eval/evaluate.cpp                            \
	eval/nnue/evaluate_nnue.cpp                  \
	eval/nnue/features/half_kp.cpp               \
	engine/yaneuraou-engine/yaneuraou-search.cpp

OBJECTS = $(addprefix $(OBJDIR)/, $(SOURCES:.cpp=.o))
DEPENDS = $(OBJECTS:.o=.d)

# --- ビルドターゲット

.PHONY : all normal tournament profgen profuse pgo clean

all: clean normal

$(TARGET): $(OBJECTS)
	$(COMPILER) -o $@ $^ $(LDFLAGS) $(CPPFLAGS)

$(OBJDIR)/%.o: %.cpp
	@[ -d $(dir $@) ] || mkdir -p $(dir $@)
	$(COMPILER) $(CPPFLAGS) -o $@ -c $<

# 通常使用。
normal:
	$(MAKE) CPPFLAGS='$(CPPFLAGS) $(LTOFLAGS)' LDFLAGS='$(LDFLAGS) $(LTOFLAGS)' $(TARGET)

# トーナメント用
tournament:
	$(MAKE) CPPFLAGS='$(CPPFLAGS) $(LTOFLAGS) -DFOR_TOURNAMENT' LDFLAGS='$(LDFLAGS) $(LTOFLAGS)' $(TARGET)

# PGO (profile-guided optimization)。g++用。
profgen:
	$(MAKE) CPPFLAGS='$(CPPFLAGS) -fprofile-generate -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-generate -lgcov' $(TARGET)

profuse:
	$(MAKE) CPPFLAGS='$(CPPFLAGS) $(LTOFLAGS) -fprofile-use -lgcov' LDFLAGS='$(LDFLAGS) -fprofile-use -lgcov $(LTOFLAGS)' $(TARGET)

pgo:
	$(MAKE) clean
	$(MAKE) profgen
	@./$(TARGET) EvalDir $(EVAL_DIR) , bench , quit
	@touch $(SOURCES)
	$(MAKE) profuse

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(TARGET) $(OBJECTS:.o=.gcda)

-include $(DEPENDS)
