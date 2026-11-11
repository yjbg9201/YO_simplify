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



	// n手詰め
}


} // namespace YaneuraOu

#endif // MATE_H_INCLUDED
