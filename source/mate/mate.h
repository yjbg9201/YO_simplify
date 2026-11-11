#ifndef MATE_H_INCLUDED
#define MATE_H_INCLUDED

#include "../types.h"

#include <vector>
#include <memory> // std::unique_ptr<>
#include <atomic> // std::atomic<>

#include "../thread.h"

namespace YaneuraOu {

namespace Mate {

	// Mate関連で使うテーブルの初期化
	// ※　Bitboard::init()から呼び出される。
	void init();

	// 現局面で1手詰めであるかを判定する。1手詰めであればその指し手を返す。
	// ただし1手詰めであれば確実に詰ませられるわけではなく、簡単に判定できそうな近接王手による
	// 1手詰めのみを判定する。(要するに判定に漏れがある。)
	// 詰みがある場合は、その指し手を返す。詰みがない場合は、MOVE_NONEが返る。
	// 王手がかかっている局面で呼び出してはならない。(この関数は、王手を回避しながらの詰みを探すわけではない)
	Move mate_1ply(const Position& pos);

	// mate_1ply()の手番をtemplateにしたやつ。
	template <Color Us>
	Move mate_1ply_imp(const Position& pos);

	// 利きのある場所への取れない近接王手からの3手詰め
	// 詰みがある場合は、その1手目の指し手を返す。詰みがない場合は、MOVE_NONEが返る。
	Move weak_mate_3ply(const Position& pos, int ply);

	// Mate::MateRepetition() で千日手判定を行った時の返し値
	enum class MateRepetitionState
	{
		Mate   , // 詰み扱い
		Mated  , // 詰まされた扱い
		Unknown, // そのいずれでもない
	};

	// MateSolverやMateDfpnSolverで千日手判定のために遡る最大手数
	constexpr int MAX_REPETITION_PLY = 16;

	// 連続王手などの千日手判定を行う。
	// plies_from_root : 詰み探索開始局面からの手数
	// repetition_ply  : 千日手判定を行う最大手数 (16ぐらいでいいと思う)
	// or_node         : 攻め方のnodeであればtrue。受け方のnodeであればfalse。
	MateRepetitionState mate_repetition(const Position& pos, const int plies_from_root , const int repetition_ply, bool or_node);



	// n手詰め
}


} // namespace YaneuraOu

#endif // MATE_H_INCLUDED
