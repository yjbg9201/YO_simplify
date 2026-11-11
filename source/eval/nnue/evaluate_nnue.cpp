// NNUE評価関数の計算に関するコード

#include "../../config.h"


#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

#include "../../types.h"
#include "../../evaluate.h"
#include "../../position.h"
#include "../../memory.h"
#include "../../usi.h"


#include "evaluate_nnue.h"
#include "nnue_common.h"

namespace YaneuraOu::Eval::NNUE {
extern int FV_SCALE;
}
 
// ============================================================
//              旧評価関数のためのヘルパー
// ============================================================

using namespace YaneuraOu;
void add_options_(OptionsMap& options, ThreadPool& threads);

namespace {
YaneuraOu::OptionsMap* options_ptr;
YaneuraOu::ThreadPool* threads_ptr;
}

// 📌 旧Options、旧Threadsとの互換性のための共通のマクロ 📌
#define Options (*options_ptr)
#define Threads (*threads_ptr)

namespace YaneuraOu::Eval {
void add_options(OptionsMap& options, ThreadPool& threads) {
    options_ptr = &options;
    threads_ptr = &threads;
    add_options_(options, threads);
}
}
// ============================================================

// 評価関数を読み込み済みであるか
bool        eval_loaded   = false;
std::string last_eval_dir = "None";

// 📌 この評価関数で追加したいエンジンオプションはここで追加する。
void add_options_(OptionsMap& options, ThreadPool& threads) {

    const char* default_eval_dir = "eval";
    Options.add("EvalDir", Option(default_eval_dir, [](const Option& o) {
                    std::string eval_dir = std::string(o);
                    if (last_eval_dir != eval_dir)
                    {
                        // 評価関数フォルダ名の変更に際して、評価関数ファイルの読み込みフラグをクリアする。
                        last_eval_dir = eval_dir;
                        eval_loaded   = false;
                    }
                    return std::nullopt;
                }));


    // NNUEのFV_SCALEの値
    Options.add("FV_SCALE", Option(16, 1, 128, [&](const Option& o) {
                    YaneuraOu::Eval::NNUE::FV_SCALE = int(o);
                    return std::nullopt;
                }));

}

namespace YaneuraOu {
namespace Eval {
namespace NNUE {

	int FV_SCALE = 16; // 水匠5では24がベストらしいのでエンジンオプション"FV_SCALE"で変更可能にした。


    // NNUE評価関数パラメーター（共有メモリまたはローカルメモリ上に配置）
    SystemWideSharedConstant<NnueNetworks> shared_networks;

    // 評価関数ファイル名
    const char* const kFileName = EvalFileDefaultName;


    // 評価関数の構造を表す文字列を取得する
    std::string GetArchitectureString() {
        const std::string base = "Features=" + FeatureTransformer::GetStructureString() +
			",Network=" + Network::GetStructureString();
		return base;
    }

namespace {
	namespace Detail {

		// 評価関数パラメータを読み込む（参照版）
		template <typename T>
		Tools::Result ReadParameters(std::istream& stream, T& obj) {
			std::uint32_t header;
			stream.read(reinterpret_cast<char*>(&header), sizeof(header));
			if (!stream) return Tools::ResultCode::FileReadError;
			// hash値、古い評価関数ファイルに対して一致するとは限らないので、警告に変更する。
			if (header != T::GetHashValue())
				sync_cout << "info string Warning : nn.bin hash mismatch." << sync_endl;
			return obj.ReadParameters(stream);
		}

	}  // namespace Detail

	// テンポラリにパラメータを読み込み、共有メモリに配置する。
	// 同じパラメータを持つ他プロセスが既に共有メモリを作成済みなら、そちらを参照する。
	Tools::Result LoadAndShare(std::istream& stream, [[maybe_unused]] std::string* failed_file) {
		// テンポラリ領域にパラメータを読み込む
		auto tmp = make_unique_large_page<NnueNetworks>();

		std::uint32_t hash_value;
		std::string architecture;
		Tools::Result result = ReadHeader(stream, &hash_value, &architecture, nullptr);
		if (result.is_not_ok()) return result;
		if (hash_value != kHashValue) {
			sync_cout << "info string Warning: NNUE hash mismatch: expected " << kHashValue
				<< " got " << hash_value
				<< " arch_in_file=" << architecture
				<< " arch_expected=" << GetArchitectureString()
				<< sync_endl;
		}

		result = Detail::ReadParameters<FeatureTransformer>(stream, tmp->feature_transformer);
		if (result.is_not_ok()) {
			sync_cout << "info string NNUE feature params read failed: " << result.to_string() << sync_endl;
			return result;
		}
		for (int i = 0; i < kLayerStacks; ++i) {
			result = Detail::ReadParameters<Network>(stream, tmp->network[i]);
			if (result.is_not_ok()) {
				sync_cout << "info string NNUE network params read failed at stack " << i << ": " << result.to_string() << sync_endl;
				return result;
			}
		}

		if (!stream || stream.peek() != std::ios::traits_type::eof())
			return Tools::ResultCode::FileCloseError;

		// 共有メモリに配置（同一ハッシュの共有メモリが既に存在すればそちらを参照）
		shared_networks = SystemWideSharedConstant<NnueNetworks>(*tmp);

		return Tools::ResultCode::Ok;
	}

	}  // namespace
    // ヘッダを読み込む
    Tools::Result ReadHeader(std::istream& stream,
        std::uint32_t* hash_value, std::string* architecture, std::uint32_t* version_out) {
        std::uint32_t version = 0, size = 0;
        stream.read(reinterpret_cast<char*>(&version), sizeof(version));
        stream.read(reinterpret_cast<char*>(hash_value), sizeof(*hash_value));
        stream.read(reinterpret_cast<char*>(&size), sizeof(size));
		if (!stream) return Tools::ResultCode::FileReadError;
		if (version_out)
			*version_out = version;
        if (version != kVersion) {
			sync_cout << "info string NNUE header version mismatch: expected " << kVersion
				<< " got " << version << sync_endl;
			return Tools::ResultCode::FileMismatch;
		}
        architecture->resize(size);
        stream.read(&(*architecture)[0], size);
		return !stream.fail() ? Tools::ResultCode::Ok : Tools::ResultCode::FileReadError;
    }

    	// 評価関数パラメータを読み込む
    Tools::Result ReadParameters(std::istream& stream, std::string* failed_file) {
        return LoadAndShare(stream, failed_file);
    }

    // 差分計算ができるなら進める
    static void UpdateAccumulatorIfPossible(const Position& pos) {
        networks().feature_transformer.UpdateAccumulatorIfPossible(pos);
    }


    // 評価値を計算する
    static Value ComputeScore(const Position& pos, bool refresh = false) {
        auto& accumulator = pos.state()->accumulator;
        if (!refresh && accumulator.computed_score) {
            return accumulator.score;
        }

        alignas(kCacheLineSize) char buffer[Network::kBufferSize];
        alignas(kCacheLineSize) TransformedFeatureType
            transformed_features[FeatureTransformer::kBufferSize];
        networks().feature_transformer.Transform(pos, transformed_features, refresh);
        const auto output = networks().network[0].Propagate(transformed_features, buffer);

        // VALUE_MAX_EVALより大きな値が返ってくるとaspiration searchがfail highして
        // 探索が終わらなくなるのでVALUE_MAX_EVAL以下であることを保証すべき。

        // この現象が起きても、対局時に秒固定などだとそこで探索が打ち切られるので、
        // 1つ前のiterationのときの最善手がbestmoveとして指されるので見かけ上、
        // 問題ない。このVALUE_MAX_EVALが返ってくるような状況は、ほぼ詰みの局面であり、
        // そのような詰みの局面が出現するのは終盤で形勢に大差がついていることが多いので
        // 勝敗にはあまり影響しない。

        // しかし、教師生成時などdepth固定で探索するときに探索から戻ってこなくなるので
        // そのスレッドの計算時間を無駄にする。またdepth固定対局でtime-outするようになる。

        auto score = static_cast<Value>(output[0] / FV_SCALE);

        // 1) ここ、下手にclipすると学習時には影響があるような気もするが…。
        // 2) accumulator.scoreは、差分計算の時に用いないので書き換えて問題ない。
        score = Math::clamp(score, -VALUE_MAX_EVAL, VALUE_MAX_EVAL);

        accumulator.score = score;
        accumulator.computed_score = true;
        return accumulator.score;
    }

}  // namespace NNUE


// 評価関数ファイルを読み込む
void load_eval() {
    // 評価関数パラメーターを読み込み済みであるなら帰る。
    if (eval_loaded)
        return;

    {
        const std::string dir_name = Options["EvalDir"];
		const std::string file_name = NNUE::kFileName;
        std::string failed_file = file_name;
        const Tools::Result result = [&] {
            {
                auto abs_eval_path = Path::Combine(Directory::GetBinaryFolder(), dir_name);
                const std::string file_path = Path::Combine(abs_eval_path, file_name);
                failed_file = file_path;
                std::ifstream stream(file_path, std::ios::binary);
                sync_cout << "info string loading eval file : " << file_path << sync_endl;
				if (!stream.is_open())
					return Tools::Result(Tools::ResultCode::FileNotFound);

                return NNUE::ReadParameters(stream, &failed_file);
            }
        }();

        //      ASSERT(result);

        if (result.is_not_ok())
        {
            // 読み込みエラーのとき終了してくれないと困る。
            sync_cout << "Error! : failed to read " << failed_file << " : " << result.to_string() << sync_endl;
            Tools::exit();
        }

		// 評価関数ファイルの読み込みが完了した。
		eval_loaded = true;
    }
}


// 評価関数。差分計算ではなく全計算する。
// Position::set()で一度だけ呼び出される。(以降は差分計算)
// 手番側から見た評価値を返すので注意。(他の評価関数とは設計がこの点において異なる)
// なので、この関数の最適化は頑張らない。
Value compute_eval(const Position& pos) {
    return NNUE::ComputeScore(pos, true);
}

// 評価関数
Value evaluate(const Position& pos) {
    const auto& accumulator = pos.state()->accumulator;
    if (accumulator.computed_score) {
        return accumulator.score;
    }



    Value score = NNUE::ComputeScore(pos);

    return score;
}

} // namespace Eval
} // namespace YaneuraOu

