#include "../types.h"
#include "../position.h"
#include "../evaluate.h"
#include "../misc.h"
#include <map>

namespace YaneuraOu {
namespace Eval
{

	// 何らかの評価関数を用いる以上、駒割りの計算は必須。
	// 評価関数を一切呼び出さないならこの計算は要らないが、
	// 実行時のオーバーヘッドは小さいので、そこまで考慮することもなさげ。

	// 駒割りの計算
	// 先手側から見た評価値
	Value material(const Position& pos)
	{
		int v = VALUE_ZERO;

		for (auto i : SQ)
			v += PieceValueM[pos.piece_on(i)];

		// 手駒も足しておく
		for (auto c : COLOR)
			for (auto pt = PAWN; pt < PIECE_HAND_NB; ++pt)
				v += (c == BLACK ? 1 : -1) * Value(hand_count(pos.hand_of(c), pt) * PieceValue[pt]);

		return (Value)v;
	}


	// 内部で保持しているpieceListFb[]が正しいBonaPieceであるかを検査する。
	// 注 : デバッグ用。遅い。
	bool EvalList::is_valid(const Position& pos)
	{
		// 各駒種の手駒の最大枚数
		int hand_max[KING] = { 0,18/*歩*/,4/*香*/,4/*桂*/,4/*銀*/,2/*角*/,2/*飛*/,4/*金*/ };

		for (int i = 0; i < length() ; ++i)
		{
			BonaPiece fb = pieceListFb[i];
			// このfbが本当に存在するかをPositionクラスのほうに調べに行く。

			// 範囲外
			if (!( 0 <= fb && fb < fe_end))
				return false;

			if (fb < fe_hand_end)
			{
				// 手駒なので手駒のほう調べに行く。
				for(auto c : COLOR)
					for (PieceType pr = PAWN; pr < KING; ++pr)
					{
						// 駒ptの手駒のBonaPieceの開始番号
						auto s = kpp_hand_index[c][pr].fb;
						if (s <= fb && fb < s + hand_max[pr])
						{
							// 見つかったのでこの駒の手駒の枚数がnに一致するか調べる。
							// ただしFV38だと手駒の枚数だけBonaPieceが存在する。
							// 例えば、先手が歩を5枚持っているならBonaPieceとして
							// f_hand_pawn,f_hand_pawn+1,..,f_hand_pawn+4がpieceListFbに存在する。
							int n = (int)(fb - s) + 1;
							if (hand_count(pos.hand_of(c),pr) < n)
								return false;

							goto Found;
						}
					}
			}
			else {
				// 盤上の駒なのでこの駒が本当に存在するか調べにいく。
				for (Piece pc = NO_PIECE; pc < PIECE_NB; ++pc)
				{
					PieceType pt = type_of(pc);
					if (pt == NO_PIECE_TYPE || pt == GOLDS) // 存在しない駒
						continue;

					// 駒pcのBonaPieceの開始番号
					auto s = BonaPiece(kpp_board_index[pc].fb);
					if (s <= fb && fb < s + SQ_NB)
					{
						// 見つかったのでこの駒がsqの地点にあるかを調べる。
						Square sq = (Square)(fb - s);
						Piece pc2 = pos.piece_on(sq);

						// BonaPieceでは、歩成,成香,成桂,成銀も金扱いなので、
						// 盤上の駒がこれらであるなら金に変更しておく。
						PieceType pt2 = type_of(pc2);
						if (pt2 == PRO_PAWN || pt2 == PRO_LANCE || pt2 == PRO_KNIGHT || pt2 == PRO_SILVER)
							pc2 = make_piece(color_of(pc2), GOLD);

						if (pc2 != pc)
							return false;

						goto Found;
					}
				}
			}
			// 何故か存在しない駒であった..
			return false;
		Found:;
		}

		return true;
	}

} // namespace Eval
} // namespace YaneuraOu
