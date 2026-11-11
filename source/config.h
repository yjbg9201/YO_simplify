#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

// ============================================================
//
//  YO_simplify
//
//  やねうら王(YaneuraOu)から、対局・検討に必要な機能だけを残した簡素版。
//  探索部はやねうら王の通常探索部、評価関数は標準NNUE(HalfKP 256x2-32-32)のみ。
//
//  オリジナル : https://github.com/yaneurao/YaneuraOu
//
// ============================================================

#include <cstdint>

// 思考エンジンのバージョンとしてUSIプロトコルの"usi"コマンドに応答するときの文字列。
// ただし、この値を数値として使用することがあるので数値化できる文字列にしておく必要がある。
#if !defined(ENGINE_VERSION)
#define ENGINE_VERSION "9.80git"
#endif

// --------------------
//  CPUの種類、CPU拡張命令の使用の有無
// --------------------

// 通常はMakefile(またはVisual Studioのプロジェクト設定)から -DUSE_AVX2 のように指定する。
//
// USE_AVX512VNNI : AVX-512かつ、VNNI命令対応(Cascade Lake以降)でサポートされた命令を使うか。
// USE_AVX512     : AVX-512(サーバー向けSkylake以降)でサポートされた命令を使うか。
// USE_AVX2       : AVX2(Haswell以降)でサポートされた命令を使うか。pextなど。
// USE_SSE42      : SSE4.2でサポートされた命令を使うか。popcnt命令など。
// USE_SSE41      : SSE4.1でサポートされた命令を使うか。_mm_testz_si128など。
// USE_SSSE3      : SSSE3でサポートされた命令を使うか。
// USE_SSE2       : SSE2  でサポートされた命令を使うか。
// USE_NEON       : ARMのNEON命令を使うか。(USE_NEON_DOTPRODでdot product命令も使う)
//
// SSE2 ⊂ SSSE3 ⊂ SSE4.1 ⊂ SSE4.2 ⊂ AVX2 ⊂ AVX-512

// AVX2環境でBMI2が使えるときに、BMI2対応命令を使うのか。
// AMDのRyzenシリーズ(ZEN1/ZEN2)では、BMI2命令が遅いので、これを定義しないほうが速い。
//#define USE_BMI2

// ---------------------
//  ビルド時に選択できる設定
// ---------------------

// エンジンオプションをコンパイル時に指定したい時に用いる。
// ";"で区切って複数指定できる。
// #define ENGINE_OPTIONS "FV_SCALE=24;BookFile=no_book"

// トーナメント(大会)用のビルド。ASSERTなし、千日手検出を簡略化。
// 💡 Makefileの"tournament"ターゲットがこれを定義する。
// #define FOR_TOURNAMENT

// 千日手検出を簡略化する
// 💡同一局面の4回目の出現ではなく、2回目の出現で千日手として扱う。
//    これを無効化するとR5～10程度弱くなるが、これをオンにすると優等局面で評価値31111が出力されたりするので、
//    検討目的なら、これをオンにするのは好ましくないかも知れない。
// #define ENABLE_QUICK_DRAW

// assertのレベルを6段階で。
//  ASSERT_LV 0 : assertなし(全体的な処理が速い)
//  ASSERT_LV 1 : 軽量なassert
//  　　　…
//  ASSERT_LV 5 : 重度のassert(全体的な処理が遅い)
//#define ASSERT_LV 3

// ASSERTのリダイレクト
// ASSERTに引っかかったときに、それを"Error : x=1"のように標準出力に出力する。
//#define USE_DEBUG_ASSERT

// ===============================================================
// ここ以降では、↑↑↑で設定した内容に基づき必要なdefineを行う。
// ===============================================================

// 64bit環境専用。
static_assert(sizeof(void*) == 8, "YO_simplify supports 64-bit targets only.");
constexpr bool Is64Bit = true;

// 通常探索時の最大探索深さ
constexpr int MAX_PLY_NUM = 246;

// デバッグ時の標準出力への局面表示などに日本語文字列を用いる。
constexpr bool pretty_jp = true;

// トーナメント(大会)用に、対局に不要なものをすべて削ぎ落とす。
#if defined(FOR_TOURNAMENT)
	#undef ASSERT_LV

	// 千日手検出を簡略化する
	#define ENABLE_QUICK_DRAW
#endif

// --- assertion tools

// DEBUGビルドでないとassertが無効化されてしまうので無効化されないASSERT
// 故意にメモリアクセス違反を起こすコード。
// USE_DEBUG_ASSERTが有効なときには、ASSERTの内容を出力したあと、3秒待ってから
// アクセス違反になるようなコードを実行する。
#if !defined (USE_DEBUG_ASSERT)
#define ASSERT(X) { if (!(X)) *(int*)1 = 0; }
#else
#include <iostream>
#include <chrono>
#include <thread>
#define ASSERT(X) { if (!(X)) { std::cout << "\nError : ASSERT(" << #X << "), " << __FILE__ << "(" << __LINE__ << "): " << __func__ << std::endl; \
 std::this_thread::sleep_for(std::chrono::microseconds(3000)); *(int*)1 =0;} }
#endif

// ASSERT LVに応じたassert
#if !defined(ASSERT_LV)
#define ASSERT_LV 0
#endif

#define ASSERT_LV_EX(L, X) { if (L <= ASSERT_LV) ASSERT(X); }
#define ASSERT_LV1(X) ASSERT_LV_EX(1, X)
#define ASSERT_LV2(X) ASSERT_LV_EX(2, X)
#define ASSERT_LV3(X) ASSERT_LV_EX(3, X)
#define ASSERT_LV4(X) ASSERT_LV_EX(4, X)
#define ASSERT_LV5(X) ASSERT_LV_EX(5, X)

// memoryがalignされているかのassert
#define ASSERT_ALIGNED(ptr, alignment) assert(reinterpret_cast<uintptr_t>(ptr) % alignment == 0)

// --- declaration of unreachablity

// switchにおいてdefaultに到達しないことを明示して高速化させる
// 💡 デバッグ時は普通にしとかないと変なアドレスにジャンプして原因究明に時間がかかる。

#if defined(_MSC_VER)
#define UNREACHABLE ASSERT_LV3(false); __assume(0);
#elif defined(__GNUC__)
#define UNREACHABLE ASSERT_LV3(false); __builtin_unreachable();
#else
#define UNREACHABLE ASSERT_LV3(false);
#endif

// --- hash key

// 局面のhash keyは64bit。
namespace YaneuraOu {
using Key64 = uint64_t;
using Key   = Key64;
}

// ----------------------------
//      CPU environment
// ----------------------------

// TARGET_CPU、Makefileのほうで"ZEN2"のようにダブルコーテーション有りの文字列として定義されているはずだが、
// それが定義されていないならここでUSE_XXXオプションから推定する。
#if !defined(TARGET_CPU)
	#if defined(USE_BMI2)
	#define BMI2_STR "BMI2"
	#else
	#define BMI2_STR ""
	#endif

	#if defined(USE_AVX512VNNI)
	#define TARGET_CPU "AVX512VNNI" BMI2_STR
	#elif defined(USE_AVX512)
	#define TARGET_CPU "AVX512" BMI2_STR
	#elif defined(USE_AVX2)
	#define TARGET_CPU "AVX2" BMI2_STR
	#elif defined(USE_SSE42)
	#define TARGET_CPU "SSE4.2"
	#elif defined(USE_SSE41)
	#define TARGET_CPU "SSE4.1"
	#elif defined(USE_SSSE3)
	#define TARGET_CPU "SSSE3"
	#elif defined(USE_SSE2)
	#define TARGET_CPU "SSE2"
	#elif defined(USE_NEON_DOTPROD)
	#define TARGET_CPU "ARMV8_DOTPROD"
	#elif defined(USE_NEON)
	#define TARGET_CPU "ARMV8"
	#else
	#define TARGET_CPU "generic"
	#endif
#endif

// 上位のCPUをターゲットとするなら、その下位CPUの命令はすべて使えるはずなので…。

#if defined (USE_AVX512VNNI)
#define USE_AVX512
#endif

#if defined (USE_AVX512)
#define USE_AVX2
#endif

#if defined (USE_AVX2)
#define USE_SSE42
#endif

#if defined (USE_SSE42)
#define USE_SSE41
#endif

#if defined (USE_SSE41)
#define USE_SSSE3
#endif

#if defined (USE_SSSE3)
#define USE_SSE2
#endif

// ----------------------------
//     evaluate function
// ----------------------------

// エンジン名に付与する評価関数の種類。
#define EVAL_TYPE_NAME "NNUE"

#endif // if !defined(CONFIG_H_INCLUDED)
