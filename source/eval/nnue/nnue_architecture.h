// Input features and network structure used in NNUE evaluation function
// NNUE評価関数で用いる入力特徴量とネットワーク構造

#ifndef CLASSIC_NNUE_ARCHITECTURE_H_INCLUDED
#define CLASSIC_NNUE_ARCHITECTURE_H_INCLUDED

#include "../../config.h"


// Defines the network structure
// 入力特徴量とネットワーク構造が定義されたヘッダをincludeする


// どれも定義されていなかったので標準NNUE型にしておく。
#include "architectures/halfkp_256x2-32-32.h"


namespace YaneuraOu {
namespace Eval::NNUE {

	static_assert(kTransformedFeatureDimensions % kMaxSimdWidth == 0, "");
	static_assert(Network::kOutputDimensions == 1, "");
	static_assert(std::is_same<Network::OutputType, std::int32_t>::value, "");

	// Trigger for full calculation instead of difference calculation
	// 差分計算の代わりに全計算を行うタイミングのリスト
	constexpr auto kRefreshTriggers = RawFeatures::kRefreshTriggers;

} // namespace Eval::NNUE
} // namespace YaneuraOu


#endif // #ifndef NNUE_ARCHITECTURE_H_INCLUDED
