#include "mate.h"


// clang-format off
// 🌈 indentに特別な意味があるので、clang-formatはこのファイルでは無効化しておく。

// 利きを用いない1手詰め判定用。(Bonanza6風)
// やねうら王2014からの移植。

#include "../position.h"

namespace YaneuraOu {

//#include <iostream>
//using std::cout;
using namespace BB_Table;

namespace {

	// sgn関数。C++標準になんでこんなもんすらないのか..。
	template <typename T> int sgn(T val) {
		return (T(0) < val) - (val < T(0));
	}

	// 1手詰めルーチンで用いる、移動によって王手になるかどうかの判定用テーブルで使う。
	enum PieceTypeCheck
	{
		PIECE_TYPE_CHECK_PAWN_WITH_NO_PRO, // 不成りのまま王手になるところ(成れる場合は含まず)
		PIECE_TYPE_CHECK_PAWN_WITH_PRO, // 成りで王手になるところ
		PIECE_TYPE_CHECK_LANCE,
		PIECE_TYPE_CHECK_KNIGHT,
		PIECE_TYPE_CHECK_SILVER,
		PIECE_TYPE_CHECK_GOLD,
		PIECE_TYPE_CHECK_BISHOP,
		PIECE_TYPE_CHECK_ROOK,
		PIECE_TYPE_CHECK_PRO_BISHOP,
		PIECE_TYPE_CHECK_PRO_ROOK,
		PIECE_TYPE_CHECK_NON_SLIDER, // 王手になる非遠方駒の移動元

		PIECE_TYPE_CHECK_NB,
		PIECE_TYPE_CHECK_ZERO = 0,
	};

	ENABLE_FULL_OPERATORS_ON(PieceTypeCheck);

	// 王手になる候補の駒の位置を示すBitboard
	Bitboard CHECK_CAND_BB[SQ_NB_PLUS1][PIECE_TYPE_CHECK_NB][COLOR_NB];

	// 玉周辺の利きを求めるときに使う、玉周辺に利きをつける候補の駒を表すBB
	// COLORのところは王手する側の駒
	Bitboard CHECK_AROUND_BB[SQ_NB_PLUS1][PIECE_RAW_NB][COLOR_NB];

	// 移動により王手になるbitboardを返す。
	// us側が王手する。sq_king = 敵玉の升。pc = 駒
	inline Bitboard check_cand_bb(Color us, PieceTypeCheck pc, Square sq_king)
	{
		return CHECK_CAND_BB[sq_king][pc][us];
	}

	// ↑のtemplate版
	template <Color Us>
	inline Bitboard check_cand_bb(PieceTypeCheck pc, Square sq_king)
	{
		return CHECK_CAND_BB[sq_king][pc][Us];
	}

	// 敵玉8近傍の利きに関係する自駒の候補のbitboardを返す。ここになければ玉周辺に利きをつけない。
	// pt = PAWN～HDK
	inline Bitboard check_around_bb(Color us, PieceType pt, Square sq_king)
	{
		return CHECK_AROUND_BB[sq_king][pt - 1][us];
	}

	// ↑のtemplate版
	template <Color Us>
	inline Bitboard check_around_bb(PieceType pt, Square sq_king)
	{
		return CHECK_AROUND_BB[sq_king][pt - 1][Us];
	}

	// sq1に対してsq2の升の延長上にある次の升を得る。
	// 隣接していないか、盤外になるときはSQUARE_NB
	// テーブルサイズを小さくしておきたいのでu8にしておく。
	/*Square*/ u8 NextSquare[SQ_NB_PLUS1][SQ_NB_PLUS1];
	inline Square nextSquare(Square sq1, Square sq2) { return (Square)NextSquare[sq1][sq2]; }

	// 上で宣言してある一連のテーブルの初期化。結構たいへん。
	void init_check_bb()
	{
		for (PieceTypeCheck p = PIECE_TYPE_CHECK_ZERO; p < PIECE_TYPE_CHECK_NB; ++p)
			for (auto sq : SQ)
				for (auto c : COLOR)
				{
					Bitboard bb(ZERO), tmp(ZERO);
					Square to;

					// 敵陣
					Bitboard enemyBB = enemy_field(c);

					// 敵陣+1段
					// Bitboard enemy4BB = c == BLACK ? RANK1_4BB : RANK6_9BB;

					switch ((int)p)
					{
					case PIECE_TYPE_CHECK_PAWN_WITH_NO_PRO:
						// 歩が不成りで王手になるところだけ。

						bb = pawnEffect(~c, sq) & ~enemyBB;
						if (!bb)
							break;
						to = bb.pop();
						bb = pawnEffect(~c, to);
						break;

					case PIECE_TYPE_CHECK_PAWN_WITH_PRO:

						bb = goldEffect(~c, sq) & enemy_field(c);
						bb = pawnBbEffect(~c, bb);
						break;

					case PIECE_TYPE_CHECK_LANCE:

						// 成りによるものもあるからな..候補だけ列挙しておくか。
						bb = lanceStepEffect(~c, sq);
						if (enemy_field(c) & sq)
						{
							// 敵陣なので成りで王手できるから、sqより下段の香も足さないと。
							if (file_of(sq) != FILE_1)
								bb |= lanceStepEffect(~c, sq + SQ_R);
							if (file_of(sq) != FILE_9)
								bb |= lanceStepEffect(~c, sq + SQ_L);
						}

						break;

					case PIECE_TYPE_CHECK_KNIGHT:

						// 敵玉から桂の桂にある駒
						tmp = knightEffect(~c, sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= knightEffect(~c, to);
						}
						// 成って王手(金)になる移動元
						tmp = goldEffect(~c, sq) & enemyBB;
						while (tmp)
						{
							to = tmp.pop();
							bb |= knightEffect(~c, to);
						}
						break;

					case PIECE_TYPE_CHECK_SILVER:

						// 敵玉から銀の銀にある駒。
						tmp = silverEffect(~c, sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= silverEffect(~c, to);
						}
						// 成って王手の場合、敵玉から金の銀にある駒
						tmp = goldEffect(~c, sq) & enemyBB;
						while (tmp)
						{
							to = tmp.pop();
							bb |= silverEffect(~c, to);
						}
						// あと4段目の玉に3段目から成っての王手。玉のひとつ下の升とその斜めおよび、
						// 玉のひとつ下の升の2つとなりの升
						{
							Rank r = (c == BLACK ? RANK_4 : RANK_6);
							if (r == rank_of(sq))
							{
								r = (c == BLACK ? RANK_3 : RANK_7);
								to = (file_of(sq) | r);
								bb |= to;
								bb |= cross45StepEffect(to);

								// 2升隣。
								if (file_of(to) >= FILE_3)
									bb |= (to + SQ_R * 2);
								if (file_of(to) <= FILE_7)
									bb |= (to + SQ_L * 2);
							}

							// 5段目の玉に成りでのバックアタック的な..
							if (rank_of(sq) == RANK_5)
								bb |= knightEffect(c, sq);
						}
						break;

					case PIECE_TYPE_CHECK_GOLD:
						// 敵玉から金の金にある駒
						tmp = goldEffect(~c, sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= goldEffect(~c, to);
						}
						break;

						// この4枚、どうせいないときもあるわけで、効果に乏しいので要らないのでは…。
					case PIECE_TYPE_CHECK_BISHOP:
					case PIECE_TYPE_CHECK_PRO_BISHOP:
					case PIECE_TYPE_CHECK_ROOK:
					case PIECE_TYPE_CHECK_PRO_ROOK:
						// 王の8近傍の8近傍(24近傍)か、王の3列、3行か。結構の範囲なのでこれ無駄になるな…。
						break;

						// 非遠方駒の合体bitboard。ちょっとぐらい速くなるんだろう…。
					case PIECE_TYPE_CHECK_NON_SLIDER:
						bb =  CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_GOLD][c]
							| CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_KNIGHT][c]
							| CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_SILVER][c]
							| CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_PAWN_WITH_NO_PRO][c]
							| CHECK_CAND_BB[sq][PIECE_TYPE_CHECK_PAWN_WITH_PRO][c];
						break;
					}
					bb &= ~Bitboard(sq); // sqの地点邪魔なので消しておく。
					CHECK_CAND_BB[sq][p][c] = bb;
				}


		for (PieceType p = PAWN; p <= KING; ++p)
			for (auto sq : SQ)
				for (auto c : COLOR)
				{
					Bitboard bb(ZERO), tmp(ZERO);
					Square to;

					switch (p)
					{
					case PAWN:
						// これ用意するほどでもないんだな
						// 一応、用意するコード書いておくか..
						bb = kingEffect(sq);
						bb = pawnBbEffect(c, bb);
						// →　このシフトでp[0]の63bit目に来るとまずいので..
						bb &= Bitboard(1); // Bitboard(1)でand取っておく。
						break;

					case LANCE:
						// 香で玉8近傍の利きに関与するのは…。玉と同じ段より攻撃側の陣にある香だけか..
						bb = lanceStepEffect(~c, sq);
						if (file_of(sq) != FILE_1)
							bb |= lanceStepEffect(~c, sq + SQ_R) | (sq + SQ_R);
						if (file_of(sq) != FILE_9)
							bb |= lanceStepEffect(~c, sq + SQ_L) | (sq + SQ_L);
						break;

					case KNIGHT:
						// 桂は玉8近傍の逆桂か。
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= knightEffect(~c, to);
						}
						break;

					case SILVER:
						// 同じく
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= silverEffect(~c, to);
						}
						break;

					case GOLD:
						// 同じく
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= goldEffect(~c, to);
						}
						break;

					case BISHOP:
						// 同じく
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= bishopStepEffect(to);
						}
						break;

					case ROOK:
						// 同じく
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= rookStepEffect(to);
						}
						break;

						// HDK相当
					case KING:
						// 同じく
						tmp = kingEffect(sq);
						while (tmp)
						{
							to = tmp.pop();
							bb |= kingEffect(to);
						}
						break;

					default:
						UNREACHABLE;
					}

					bb &= ~Bitboard(sq); // sqの地点邪魔なので消しておく。
										 // CHECK_CAND_BBとは並び順を変えたので注意。
					CHECK_AROUND_BB[sq][p - 1][c] = bb;
				}

		// NextSquareの初期化
		// Square NextSquare[SQUARE_NB][SQUARE_NB];
		// sq1に対してsq2の升の延長上にある次の升を得る。
		// 隣接していないか、盤外になるときはSQUARE_NB

		for (auto s1 : SQ)
			for (auto s2 : SQ)
			{
				Square next_sq = SQ_NB;

				// 隣接していなくてもok。縦横斜かどうかだけ判定すべし。
				if (queenStepEffect(s1) & s2)
				{
					File vf = File(sgn(file_of(s2) - file_of(s1)));
					Rank vr = Rank(sgn(rank_of(s2) - rank_of(s1)));

					File s3f = file_of(s2) + vf;
					Rank s3r = rank_of(s2) + vr;
					// 盤面の範囲外に出ていないかのテスト
					if (is_ok(s3f) && is_ok(s3r))
						next_sq = s3f | s3r;
				}
				NextSquare[s1][s2] = next_sq;
			}

	}

	// 桂馬が次に成れる移動元の表現のために必要となるので用意。
	const Bitboard RANK3_5BB = RANK3_BB | RANK4_BB | RANK5_BB;
	const Bitboard RANK5_7BB = RANK5_BB | RANK6_BB | RANK7_BB;

	//
	//　以下、本当ならPositionに用意すべきヘルパ関数
	//

	// 上の関数群とは異なる。UsのSliderの利きを列挙する。
	// avoid升にいる駒の利きは除外される。
	template <Color Us>
	Bitboard AttacksSlider(const Position& pos, const Bitboard& slide)
	{
		Bitboard bb, sum(ZERO);
		Square from;

		bb = pos.pieces<Us, LANCE>();
		while (bb)
		{
			from = bb.pop();
			sum |= lanceEffect<Us>( from, slide);
		}
		bb = pos.pieces<Us, BISHOP_HORSE>();
		while (bb)
		{
			from = bb.pop();
			sum |= bishopEffect(from, slide);
		}
		bb = pos.pieces<Us, ROOK_DRAGON>();
		while (bb)
		{
			from = bb.pop();
			sum |= rookEffect(from, slide);
		}
		return sum;
	}

	// usのSliderの利きを列挙する
	// avoid升にいる駒の利きは除外される。
	template <Color Us>
	Bitboard AttacksSlider(const Position& pos, Square avoid_from, const Bitboard& occ)
	{
		Bitboard bb, sum(ZERO);
		Bitboard avoid_bb = ~Bitboard(avoid_from);
		Square from;

		bb = pos.pieces<Us, LANCE>() & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= lanceEffect<Us>( from, occ);
		}
		bb = pos.pieces<Us, BISHOP_HORSE>() & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= bishopEffect(from, occ);
		}
		bb = pos.pieces<Us, ROOK_DRAGON>() & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= rookEffect(from, occ);
		}
		return sum;
	}

	// NonSliderの利きのみ列挙
	template<Color OurKing>
	Bitboard AttacksAroundKingNonSlider(const Position& pos)
	{
		Square sq_king = pos.square<KING>(OurKing);
		constexpr Color Them = ~OurKing;
		Square from;
		Bitboard bb;

		// 歩は普通でいい
		Bitboard sum = pawnBbEffect<Them>(pos.pieces<Them>(PAWN));

		// ほとんどのケースにおいて候補になる駒はなく、whileで回らずに抜けると期待している。
		bb = pos.pieces<Them,KNIGHT>() & check_around_bb<Them>(KNIGHT, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= knightEffect<Them>(from);
		}
		bb = pos.pieces<Them,SILVER>() & check_around_bb<Them>(SILVER, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= silverEffect<Them>(from);
		}
		bb = pos.pieces<Them,GOLDS>() & check_around_bb<Them>(GOLD, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= goldEffect<Them>(from);
		}
		bb = pos.pieces<Them,HDK>() & check_around_bb<Them>(KING, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= kingEffect(from);
		}
		return sum;
	}

	// Sliderの利きのみ列挙
	template <Color OurKing>
	Bitboard AttacksAroundKingSlider(const Position& pos)
	{
		constexpr Color Them = ~OurKing;

		Square   sq_king = pos.square<KING>(OurKing);
		Square from;
		Bitboard bb;
		Bitboard sum(ZERO);

		bb = pos.pieces<Them,LANCE>() & check_around_bb<Them>(LANCE, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= lanceEffect<Them>( from, pos.pieces());
		}
		bb = pos.pieces<Them,BISHOP_HORSE>() & check_around_bb<Them>(BISHOP, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= bishopEffect(from, pos.pieces());
		}
		bb = pos.pieces<Them,ROOK_DRAGON>() & check_around_bb<Them>(ROOK, sq_king);
		while (bb)
		{
			from = bb.pop();
			sum |= rookEffect(from, pos.pieces());
		}
		return sum;
	}

	template <Color Us>
	Bitboard AttacksAroundKingNonSliderInAvoiding(const Position& pos, Square avoid_from)
	{
		Square sq_king = pos.square<KING>(Us);
		constexpr Color Them = ~Us;
		Bitboard bb;
		Bitboard avoid_bb = ~Bitboard(avoid_from);
		Square from;

		// 歩は普通でいい
		Bitboard sum = pawnBbEffect<Them>(pos.pieces<Them,PAWN>());

		// ほとんどのケースにおいて候補になる駒はなく、whileで回らずに抜けると期待している。
		bb = pos.pieces<Them,KNIGHT>() & check_around_bb<Them>(KNIGHT, sq_king) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= knightEffect<Them>(from);
		}
		bb = pos.pieces<Them,SILVER>() & check_around_bb<Them>(SILVER, sq_king) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= silverEffect<Them>(from);
		}
		bb = pos.pieces<Them,GOLDS>() & check_around_bb<Them>(GOLD, sq_king) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= goldEffect<Them>(from);
		}
		bb = pos.pieces<Them,HDK>() & check_around_bb<Them>(KING, sq_king) & avoid_bb;
		while (bb)
		{
			from = bb.pop();
			sum |= kingEffect(from);
		}
		return sum;
	}


	// avoidの駒の利きだけは無視して玉周辺の敵の利きを考えるバージョン。
	// この関数ではわからないため、toの地点から発生する利きはこの関数では感知しない。
	// 王手がかかっている局面において逃げ場所を見るときに裏側からのpinnerによる攻撃を考慮して、玉はいないものとして
	// 考える必要があることに注意せよ。(slide = pos.slide() ^ from ^ king | to) みたいなコードが必要
	// avoidの駒の利きだけは無視して玉周辺の利きを考えるバージョン。
	template <Color Us>
	inline Bitboard AttacksAroundKingInAvoiding(const Position& pos, Square from, const Bitboard& occ)
	{
		return AttacksAroundKingNonSliderInAvoiding<Us>(pos, from) | AttacksSlider<~Us>(pos, from, occ);
	}

	// 歩が打てるかの判定用。
	// 歩を持っているかの判定も含む。
	template<Color Us>
	inline bool can_pawn_drop(const Position& pos, Square sq)
	{
		// 歩を持っていて、二歩ではない。
		return hand_count(pos.hand_of<Us>(), PAWN) > 0 && !((pos.pieces<Us,PAWN>() & FILE_BB[file_of(sq)]));
	}

}

using namespace Effect8;

namespace {

	// kingがtoとbb_avoid以外の升に逃げられるか
	// toに駒がない場合、駒が打たれているのでこれによって升は遮断されているものとして考える。
	template <Color Us>
	bool can_king_escape(const Position& pos, Square to, const Bitboard& bb_avoid, const Bitboard& slide_)
	{
		// toには駒が置かれているのでこれにより利きの遮断は発生している。(attackers_to()で利きを見るときに重要)
		// captureの場合、もともとtoには駒があるわけで、ここをxorで処理するわけにはいかない。
		Bitboard slide = slide_ | to;

		Square sq_king = pos.square<KING>(Us);
		/*
		// kingもいないものとして考える必要がある。
		slide ^= sq_king;
		// これは呼び出し側でbb_avoidを計算するときに保証するものとする。
		*/

		// bbとtoと自駒のないところから移動先を探す
		Bitboard bb = (bb_avoid | to | pos.pieces(Us)).andnot(kingEffect(sq_king));

		while (bb)
		{
			Square escape = bb.pop();

			if (!pos.attackers_to<~Us>(escape, slide))
				return true;
			// 何も破壊していないので即座に返って良い。

		}
		return false;
	}

	// kingがtoとbb_avoid以外の升に逃げられるか
	// toに駒がない場合、駒が打たれているのでこれによって升は遮断されているものとして考える。
	// またfromからは駒が除去されているものとして考える。
	template <Color Us>
	bool can_king_escape(const Position& pos, Square from, Square to, const Bitboard& bb_avoid, const Bitboard& slide_)
	{
		// toには駒が置かれているのでこれにより利きの遮断は発生している。(attackers_to()で利きを見るときに重要)
		Bitboard slide = slide_ | to;

		Square sq_king = pos.square<KING>(Us);
		// kingもいないものとして考える必要がある。
		slide ^= sq_king;
		// これは呼び出し側でbb_avoidを計算するときに保証するものとする。
		// →　ああ、だめだ。fromの後ろにあった駒での開き王手が..

		// bb_avoidとtoと自駒のないところから移動先を探す
		Bitboard bb = (bb_avoid | to | pos.pieces(Us)).andnot(kingEffect(sq_king));

		while (bb)
		{
			Square escape = bb.pop();

			if (!(Bitboard(from).andnot(pos.attackers_to<~Us>(escape, slide) )))
				// fromにある攻撃駒は移動済なのでこれは対象外。
				return true;
			// 何も破壊していないので即座に返って良い。

		}
		return false;
	}

	// kingがbb_avoid以外の升に逃げられるか
	// toに駒がない場合、駒が打たれているのでこれによって升は遮断されているものとして考える。
	// またfromからは駒が除去されているものとして考える。
	// ただしtoには行けるものとする。
	template <Color Us>
	bool can_king_escape_cangoto(const Position& pos, Square from, Square to, const Bitboard& bb_avoid, const Bitboard& slide_)
	{
		constexpr Color Them = ~Us;

		// toには駒が置かれているのでこれにより利きの遮断は発生している。(attackers_to()で利きを見るときに重要)
		Bitboard slide = slide_ | to;

		Square sq_king = pos.square<KING>(Us);
		// kingもいないものとして考える必要がある。
		slide ^= sq_king;
		// これは呼び出し側でbb_avoidを計算するときに保証するものとする。
		// →　ああ、だめだ。fromの後ろにあった駒での開き王手が..

		// bb_avoid/*とto*/と自駒のないところから移動先を探す

		// Bitboard bb = kingEffect(sq_king) & ~((bb_avoid /*| to*/ | pos.pieces<Us>()) & ~Bitboard(to));
		Bitboard bb = ((bb_avoid /*| to*/ | pos.pieces<Us>()).andnot(Bitboard(to).andnot(kingEffect(sq_king))));

		// toには移動できるのだよ。pos.pieces(us)には玉側の駒がtoにあることがあって、これは取られるものとして
		// 考える必要があるから、toを除外するコードが必要なのだよ。

		while (bb)
		{
			Square escape = bb.pop();

			if (!(Bitboard(from).andnot(pos.attackers_to<Them>(escape, slide))))
				// fromにある攻撃駒は移動済なのでこれは対象外。
				return true;
			// 何も破壊していないので即座に返って良い。

		}
		return false;
	}

	// 玉以外の駒でtoの駒が取れるのか？(toの地点には敵の利きがある or 届かないので玉では取れないものとする)
	template <Color Us>
	bool can_piece_capture(const Position& pos, Square to, const Bitboard& pinned, const Bitboard& slide)
	{
		Square sq_king = pos.square<KING>(Us);

		// 玉以外の駒でこれが取れるのか？(toの地点には敵の利きがある or 届かないので玉では取れないものとする)
		Bitboard sum = pos.pieces(KING).andnot(pos.attackers_to<Us>(to, slide));
		while (sum)
		{
			Square from = sum.pop();

			// fromからtoに移動させて素抜きに合わないならばこれをもって良し。
			if (!pinned
				|| !(pinned & from)
				|| aligned(from, to, sq_king)
				)
				return true;
		}

		return false;
	}

	// toにある駒が捕獲できるのか
	// ただしavoid升の駒でのcaptureは除外する。
	template <Color Us>
	bool can_piece_capture(const Position& pos, Square to, Square avoid, const Bitboard& pinned, const Bitboard& slide)
	{
		ASSERT_LV3(is_ok(to));

		Square sq_king = pos.square<KING>(Us);

		// 玉以外の駒でこれが取れるのか？(toの地点には敵の利きがあるので玉では取れないものとする)
		Bitboard sum = (pos.pieces(KING) | Bitboard(avoid)).andnot(pos.attackers_to<Us>(to, slide));
		while (sum)
		{
			Square from = sum.pop();

			// fromからtoに移動させて素抜きに合わないならばこれをもって良し。
			if (!pinned
				|| !(pinned & from)
				|| aligned(from, to, sq_king)
				)
				return true;
		}

		return false;
	}

}

namespace Mate {

	// Mate1Ply関係のテーブル初期化
	void init_mate_1ply()
	{
		// CHECK_CAND_BB、CHECK_AROUND_BBの初期化
		init_check_bb();
	}

	// 1手で詰むならばその指し手を返す。なければMOVE_NONEを返す
	template <Color Us>
	Move mate_1ply_imp(const Position& pos)
	{
		ASSERT_LV3(!pos.checkers());

		constexpr Color Them = ~Us;
        Square          sq_king = pos.square<KING>(Them);

		// 移動させると(相手側＝非手番側)の玉に対して空き王手となる候補の(手番側)駒のbitboard。
        Bitboard dcCandidates = pos.blockers_for_king(Them) & pos.pieces(Us);

		// 相手玉側のpinされている駒の列挙(相手玉側は、この駒を動かすと素抜きに遭う)
        Bitboard pinned = pos.blockers_for_king<Them>() & pos.pieces(Them);
	
		Square from, to;

		// -- 駒打ちによる即詰み

		// 駒が打てる場所
		Bitboard bb_drop = ~pos.pieces();

		// テンポラリ用
		Bitboard bb;

		// 攻撃範囲計算用
		Bitboard bb_attacks;

		Hand ourHand  = pos.hand_of<Us  >();
		Hand themHand = pos.hand_of<Them>();

		// 飛車を短く打つ場合
		if (hand_count(ourHand, ROOK))
		{
			// 敵玉の上下左右の駒の打てる場所
			bb = rookStepEffect(sq_king) & kingEffect(sq_king) & bb_drop;

			while (bb)
			{
				to = bb.pop();
				// toに対して自駒が利いてないと意味ない
				if (!pos.attackers_to<Us>(to))
					continue;

				// このtoに飛車を打つものとして…この十字方向には逃げられないわけだから…そこは駄目ですよ、と。
				bb_attacks = rookStepEffect(to);

				if (can_king_escape  <Them>(pos, to, bb_attacks, pos.pieces())) { continue; }
				if (can_piece_capture<Them>(pos, to, pinned    , pos.pieces())) { continue; }
				return make_move_drop(ROOK, to , Us);
			}
		}

		// 香を短く打つ場合
		if (hand_count(ourHand, LANCE))
		{
			bb = pawnEffect<Them>(sq_king) & bb_drop;
			if (bb)
			{
				to = bb.pop();
				if (pos.attackers_to<Us>(to))
				{
					bb_attacks = lanceStepEffect<Us>(to);
					if (can_king_escape  <Them>(pos, to, bb_attacks, pos.pieces())) { goto SKIP_LANCE; }
					if (can_piece_capture<Them>(pos, to, pinned    , pos.pieces())) { goto SKIP_LANCE; }
					return make_move_drop(LANCE, to, Us);

				SKIP_LANCE:;
				}
			}
		}

		// 角を短く打つ
		if (hand_count(ourHand, BISHOP))
		{
			// 敵玉の上下左右の駒の打てる場所
			bb = cross45StepEffect(sq_king) & bb_drop;

			while (bb)
			{
				to = bb.pop();
				// toに対して自駒が利いてないと意味ない
				if (!pos.attackers_to<Us>(to))
					continue;

				// このtoに角を打つものとして…この斜め方向には逃げられないわけだから…そこは駄目ですよ、と。
				bb_attacks = bishopStepEffect(to);

				if (can_king_escape  <Them>(pos, to, bb_attacks, pos.pieces())) { continue; }
				if (can_piece_capture<Them>(pos, to, pinned    , pos.pieces())) { continue; }
				return make_move_drop(BISHOP, to , Us);
			}
		}

		// 金打ち
		if (hand_count(ourHand, GOLD))
		{
			bb = goldEffect(Them, sq_king) & bb_drop;

			// 飛車を持っているならすでに調べた上の升は除外して良い。
			// (そこに金をおいて詰むなら飛車をおいて詰んでいるはずだから)
			if (hand_count(ourHand, ROOK))
				bb = pawnEffect<Us>(sq_king).andnot(bb);

			while (bb)
			{
				to = bb.pop();
				// toに対して自駒が利いてないと意味ない
				if (!pos.attackers_to<Us>(to))
					continue;

				bb_attacks = goldEffect<Us>(to);

				if (can_king_escape  <Them>(pos, to, bb_attacks, pos.pieces())) { continue; }
				if (can_piece_capture<Them>(pos, to, pinned    , pos.pieces())) { continue; }
				return make_move_drop(GOLD, to , Us);
			}
		}
		// 銀打ち
		if (hand_count(ourHand, SILVER))
		{
			// 金打ちをすでに調べたのであれば前方向は除外
			if (hand_count(ourHand, GOLD))
			{
				// 角打ちも調べていたのであれば銀で詰むことはない
				if (hand_count(ourHand, BISHOP))
					goto SILVER_DROP_END;

				// 前方向を除外するために金のnotを用いる。
				bb = silverEffect<Them>(sq_king) & goldEffect<Them>(sq_king).andnot(bb_drop);
			}
			else {
				bb = silverEffect<Them>(sq_king) &  bb_drop;
			}
			while (bb)
			{
				to = bb.pop();
				// toに対して自駒が利いてないと意味ない
				if (!pos.attackers_to<Us>(to))
					continue;

				bb_attacks = silverEffect<Us>(to);

				if (can_king_escape  <Them>(pos, to, bb_attacks, pos.pieces())) { continue; }
				if (can_piece_capture<Them>(pos, to, pinned    , pos.pieces())) { continue; }
				return make_move_drop(SILVER, to , Us);
			}
		}
	SILVER_DROP_END:;

		// 桂打ち
		if (hand_count(ourHand, KNIGHT))
		{
			bb = knightEffect<Them>(sq_king) & bb_drop;

			while (bb)
			{
				to = bb.pop();
				//      bb_attacks =knightEffect(Us, to);
				// 桂馬はto以外は王が1手で移動できない場所なので求めるまでもない。

				if (can_king_escape  <Them>(pos, to, Bitboard(ZERO), pos.pieces())) { continue; }
				if (can_piece_capture<Them>(pos, to, pinned        , pos.pieces())) { continue; }
				return make_move_drop(KNIGHT, to , Us);
			}
		}


		// -- 移動による1手詰め

		// 駒の移動可能な場所
                Bitboard bb_move = ~pos.pieces(Us);

		// 王手となる移動先
		Bitboard bb_check;

		// 自分のpin駒
        Bitboard our_pinned = pos.blockers_for_king<Us>() & pos.pieces(Us);

		// 自玉
        Square our_king = pos.square<KING>(Us);

		// 龍
        bb = pos.pieces(Us, DRAGON);
		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_check = dragonEffect(from, slide) & bb_move & kingEffect(sq_king);
			// 玉の8近傍への移動

			// この龍の移動によってpinが変わるので再計算が必要なのか…。
			// この龍を除外してpinされている駒を計算する必要がある。

			// 龍　^金 　^王
			// 龍突っ込んだときに金のpinが変わる
			// これ、しかし
			// 飛　龍　^金 　^王
			// こういうケースがあるので龍自体を除去しないとおかしくなるな…。

			Bitboard new_pin = pos.pinned_pieces<Them>(from);
			// ここで調べるのは近接王手だけなのでpinが解除されることはないから移動先toによるpinの解除/増加はなく、
			// 考慮しなくてよい。

			while (bb_check)
			{
				to = bb_check.pop();
				// fromの駒以外のtoへの利きがあるかを調べる。
				if (!(pos.attackers_to<Us>(to, slide) ^ from)) { continue; }

				// 王手であることは保証されている。

				// この龍の移動がそもそも合法手か？
				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }

				// 玉の斜め四方への移動か
				if (cross45StepEffect(sq_king) & to)
					bb_attacks = dragonEffect(to, slide);
				else
					bb_attacks = rookStepEffect(to) | kingEffect(to);

				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }

				// 龍によるtoが玉8近傍の場合の両王手はない。
				if (can_piece_capture<Them>(pos, to, new_pin, slide)) { continue; }
				return make_move(from, to , Us , DRAGON);
			}
		}

		// 飛
		bb = pos.pieces(Us, ROOK);
		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_check = rookEffect(from, slide) & bb_move & kingEffect(sq_king);

			// 飛車の移動前の升から王側の駒をpinすることはできない。ゆえにpinはいまのままで良い。
			// →　あ、そうでもないのか

			//        ^玉
			//     ^金
			//   飛
			// 角
			//   この場合、飛車を移動させて金があらたにpinに入るのか..

			Bitboard new_pin = pos.pinned_pieces<Them>(from);

			while (bb_check)
			{
				to = bb_check.pop();
				if (!(pos.attackers_to<Us>(to, slide) ^ from)) { continue; }

				// 敵陣へ/から の移動であれば成りなので
				if (canPromote(Us, from, to))
				{
					if (cross45StepEffect(sq_king) & to)
						bb_attacks = dragonEffect(to, slide);
					else
						bb_attacks = rookStepEffect(to) | kingEffect(to);
				}
				else
					bb_attacks = rookStepEffect(to);

				// これで王手になってないと駄目
				if (!(bb_attacks & sq_king)) { continue; }

				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }

				// 移動元で飛車の、近接王手になっている以上、pin方向と違う方向への移動であるからこれは両王手である。
				if (dcCandidates & from)
					;
				else if (can_piece_capture<Them>(pos, to, new_pin, slide)) { continue; }

				if (!canPromote(Us, from, to))
					return make_move(from, to ,Us, ROOK);
				else
					return make_move_promote(from, to, Us, ROOK);
			}
		}

		// -- 以下、同様

		// 馬
		bb = pos.pieces(Us, HORSE);
		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_check = horseEffect(from, slide) & bb_move & kingEffect(sq_king);
			// 玉の8近傍への移動

			//      ^玉
			//   ^金
			// 馬
			// 馬の移動によってpinは解除されるが、王手になることはない。
			// ゆえに、pinは以前のままで良い？

			// →　駄目

			//     ^王
			//     ^金
			//      馬
			//      香
			// こういう配置から斜めに馬が移動しての王手で金が新たにpinされる。

			Bitboard new_pin = pos.pinned_pieces<Them>(from);

			while (bb_check)
			{
				to = bb_check.pop();
				if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }

				// 王手になっていることは保証されている

				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				bb_attacks = bishopStepEffect(to) | kingEffect(to);
				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }
				// 移動元で馬だとpin方向を確認しないといけない。違う方向への移動による攻撃なら、これは両王手である。
				if ((dcCandidates & from) && !aligned(from, to, sq_king))
					;
				else if (can_piece_capture<Them>(pos, to, new_pin, slide)) { continue; }

				return make_move(from, to , Us, HORSE);
			}
		}

		// 角
		bb = pos.pieces(Us, BISHOP);
		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_check = bishopEffect(from, slide) & bb_move & kingEffect(sq_king);
			Bitboard new_pin = pos.pinned_pieces<Them>(from);

			while (bb_check)
			{
				to = bb_check.pop();
				if (!(pos.attackers_to<Us>(to, slide) ^ from)) { continue; }

				// 敵陣へ/から の移動であれば成りなので
				if (canPromote(Us, from, to))
					bb_attacks = bishopStepEffect(to) | kingEffect(to);
				else
					bb_attacks = bishopStepEffect(to);

				// これで王手になってないと駄目
				if (!(bb_attacks & sq_king)) { continue; }
                if (pos.discovered(from, to, pos.square<KING>(Us), our_pinned)) { continue; }
				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }
				// 移動元で角だとpin方向を変える王手なので、これは両王手である。
				if (dcCandidates & from)
					;
				else if (can_piece_capture<Them>(pos, to, new_pin, slide)) { continue; }

				if (!canPromote(Us, from, to))
					return make_move(from, to , Us , BISHOP);
				else
					return make_move_promote(from, to , Us , BISHOP);
			}
		}

		// 香の移動王手
        bb = check_cand_bb<Us>(PIECE_TYPE_CHECK_LANCE, sq_king) & pos.pieces(Us, LANCE);

		while (bb)
		{
			from = bb.pop();
			Bitboard slide = pos.pieces() ^ from;
			bb_attacks = lanceEffect<Us>(from, slide);

			// 金で王手になる升を列挙するか…。
			bb_check = bb_attacks & bb_move & goldEffect<Them>(sq_king);

			// 香の場合も香の移動によってpinが解除されるケースはないのでpinは初期のもので良い。

			while (bb_check)
			{
				to = bb_check.pop();

				if (canPromote(Us, to))
					bb_attacks = goldEffect<Us>(to);
				else
					bb_attacks = lanceStepEffect<Us>(to);

				if (!(bb_attacks & sq_king)) { goto LANCE_NO_PRO; }
				if (!(pos.attackers_to<Us>(to, slide) ^ from)) { goto LANCE_NO_PRO; }
				if (pos.discovered(from, to, our_king, our_pinned)) { goto LANCE_NO_PRO; }
				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { goto LANCE_NO_PRO; }
				// 成って角との両王手
				if (dcCandidates & from)
					;
				else if (can_piece_capture<Them>(pos, to, pinned, slide)) { goto LANCE_NO_PRO; }

				if (!canPromote(Us, to))
					return make_move(from, to , Us , LANCE);
				else
					return make_move_promote(from, to , Us , LANCE);

				// 敵陣で不成りで串刺しにする王手も入れなきゃ..
			LANCE_NO_PRO:;
				if ((Us == BLACK ? RANK3_BB : RANK7_BB) & to)
				{
					bb_attacks = lanceStepEffect<Us>(to);
					if (!(bb_attacks & sq_king)) { continue; }
					if (!(pos.attackers_to<Us>(to, slide) ^ from)) { continue; }
					if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
					if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }
					// 串刺しでの両王手はありえない
					if (can_piece_capture<Them>(pos, to, pinned, slide)) { continue; }
					return make_move(from, to , Us , LANCE);
				}
			}
		}

		// 離し角・飛車等で詰むかどうか。
		// →　離し角・飛車での詰み、そんなに出てこないので調べるコストに見合っていないようだ。

		// 以下、金、銀、桂、歩。ひとまとめにして判定できるが…これらのひとまとめにしたbitboardがないしな…。
		// まあ、一応、やるだけやるか…。

		bb = check_cand_bb<Us>(PIECE_TYPE_CHECK_NON_SLIDER, sq_king)
			& (pos.pieces(Us, GOLDS, SILVER, KNIGHT, PAWN));
		if (!bb)
			goto DC_CHECK;

		// 金
		bb = check_cand_bb<Us>( PIECE_TYPE_CHECK_GOLD, sq_king)  & pos.pieces(Us, GOLDS);
		while (bb)
		{
			from = bb.pop();
			bb_check = goldEffect<Us>(from) & bb_move & goldEffect<Them>(sq_king);
			// 金は成りがないのでこれで100%王手。

			if (!bb_check) { continue; }

			Bitboard slide   = pos.pieces()          ^ from;
			Bitboard new_pin = pos.pinned_pieces<Them>(from);

			while (bb_check)
			{
				to = bb_check.pop();
				if (!(pos.attackers_to<Us>(to, slide) ^ from)) { continue; }
				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				bb_attacks = goldEffect<Us>(to);
				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }
				if ((dcCandidates & from) && !aligned(from, to, sq_king))
					;
				else if (can_piece_capture<Them>(pos, to, new_pin, slide)) { continue; }
				return make_move(from, to , pos.piece_on(from) /* 金相当の駒が何かは不明。 */);
			}
		}


		// 銀は成りと不成が選択できるので少し嫌らしい
		bb = check_cand_bb(Us, PIECE_TYPE_CHECK_SILVER, sq_king)  & pos.pieces(Us, SILVER);
		while (bb)
		{
			from = bb.pop();
			bb_check = silverEffect<Us>(from) & bb_move & kingEffect(sq_king);
			// 敵玉8近傍を移動先の候補とする。金+銀の王手なので…。
			if (!bb_check) { continue; }

			Bitboard slide   = pos.pieces()          ^ from;
			Bitboard new_pin = pos.pinned_pieces<Them>(from);

			while (bb_check)
			{
				to = bb_check.pop();
				bb_attacks = silverEffect<Us>(to);
				// これで王手になってないと駄目
				if (!(bb_attacks & sq_king)) { goto PRO_SILVER; }
				if (!(pos.attackers_to<Us>(to, slide) ^ from))               { goto PRO_SILVER; }
				if (pos.discovered(from, to, our_king, our_pinned))          { goto PRO_SILVER; }
				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { goto PRO_SILVER; }
				if ((dcCandidates & from) && !aligned(from, to, sq_king))
					;
				else
					if (can_piece_capture<Them>(pos, to, new_pin, slide)) { goto PRO_SILVER; }
				// fromから移動したことにより、この背後にあった駒によって新たなpinが発生している可能性がある。
				// fromとtoと玉が直線上にない場合はpinの更新が必要。
				// これは面倒なのですべての場合で事前に新しいpinを求めることにする。

				return make_move(from, to, Us , SILVER );

			PRO_SILVER:;
				// 銀成りでの王手

				// 敵陣へ/から の移動であれば成りを選択できるので..
				if (!(canPromote(Us, from, to)))
					continue;

				bb_attacks = goldEffect(Us, to);
				if (!(bb_attacks & sq_king)) { continue; }
				if (!(pos.attackers_to<Us>(to, slide) ^ from)) { continue; }
				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }
				if ((dcCandidates & from) && !aligned(from, to, sq_king))
					;
				else if (can_piece_capture<Them>(pos, to, new_pin, slide)) { continue; }
				return make_move_promote(from, to , Us , SILVER);
			}
		}

		// 桂も成りと不成が選択できるので少し嫌らしい
        bb = check_cand_bb(Us, PIECE_TYPE_CHECK_KNIGHT, sq_king) & pos.pieces(Us, KNIGHT);

		while (bb)
		{
			from = bb.pop();
			bb_check = knightEffect<Us>(from) & bb_move;
			if (!bb_check) { continue; }

			Bitboard slide   = pos.pieces()          ^ from;
			Bitboard new_pin = pos.pinned_pieces<Them>(from);

			while (bb_check)
			{
				to = bb_check.pop();
				bb_attacks = knightEffect<Us>(to);
				// 敵陣1,2段目からのStepAttackはBitboard(ZERO)相当なのでここへの不成りが生成されることはない
				if (!(bb_attacks & sq_king)) { goto PRO_KNIGHT; }
				// 桂馬の特性上、成りと不成の二通りの王手の両方が同時に可能になることはないので以下ではcontinueで良い。
				//if (!(pos.attackers_to(Us, to, slide) ^ from)) { continue; }
				// →　この駒は取られないならそれで良い。ここへの味方の利きは不要。

				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }
				// 桂馬はpinされているなら移動で必ず両王手になっているはずである。
				if (dcCandidates & from)
					;
				else if (can_piece_capture<Them>(pos, to, new_pin, slide)) { continue; }
				return make_move(from, to , Us , KNIGHT);

			PRO_KNIGHT:;
				// 桂成りでの王手

				if (!(canPromote(Us, from, to))) { continue; }
				bb_attacks = goldEffect<Us>(to);
				if (!(bb_attacks & sq_king)) { continue; }
				if (!(pos.attackers_to<Us>(to, slide) ^ from)) { continue; }
				if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
				if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }
				// 桂馬はpinされているなら移動で必ず両王手になっているはずである。
				if (dcCandidates & from)
					;
				else if (can_piece_capture<Them>(pos, to, new_pin, slide)) { continue; }
				return make_move_promote(from, to , Us , KNIGHT);
			}
		}

		// 歩の移動による詰み
        if (check_cand_bb<Us>(PIECE_TYPE_CHECK_PAWN_WITH_NO_PRO, sq_king)
            & pos.pieces(Us, PAWN))
		{
			// 先手の歩による敵玉の王手だとすると、敵玉の一升下(SQ_D)が歩の移動先。
			to = sq_king + (Us == BLACK ? SQ_D : SQ_U);
			if (pos.piece_on(to) != NO_PIECE && color_of(pos.piece_on(to)) != Them) { goto SKIP_PAWN; }
			from = to + (Us == BLACK ? SQ_D : SQ_U);

			// 敵陣であれば成りによる詰みチェックで引っかかるだろう。
			if (canPromote(Us, to)) { goto SKIP_PAWN; }

			Bitboard slide = pos.pieces() ^ from;
			if (!(pos.attackers_to<Us>(to, slide) ^ from))                   { goto SKIP_PAWN; }
			if (pos.discovered(from, to, our_king, our_pinned))              { goto SKIP_PAWN; }
			if (can_king_escape<Them>(pos, from, to, Bitboard(ZERO), slide)) { goto SKIP_PAWN; }
			// 移動王手となるpinされている歩などはないので両王手は考慮しなくて良い。
			if (can_piece_capture<Them>(pos, to, pinned, slide))             { goto SKIP_PAWN; }
			return make_move(from, to , Us , PAWN);
		}
	SKIP_PAWN:;

		// 歩の成りによる詰み
        bb = check_cand_bb<Us>(PIECE_TYPE_CHECK_PAWN_WITH_PRO, sq_king) & pos.pieces(Us, PAWN);

		while (bb)
		{
			from = bb.pop();
			to = from + (Us == BLACK ? SQ_U : SQ_D);
			if (pos.piece_on(to) != NO_PIECE && color_of(pos.piece_on(to)) != Them) { continue; }
			bb_attacks = goldEffect<Us>(to);

			if (!(bb_attacks & sq_king)) { continue; }

			Bitboard slide = pos.pieces() ^ from;
			if (!(pos.attackers_to<Us>(to, slide) ^ from)) { continue; }
			if (pos.discovered(from, to, our_king, our_pinned)) { continue; }
			if (can_king_escape<Them>(pos, from, to, bb_attacks, slide)) { continue; }
			if (can_piece_capture<Them>(pos, to, pinned, slide)) { continue; }
			return make_move_promote(from, to , Us , PAWN);
		}

	DC_CHECK:;

		// 持将棋の判定入れておくか…。
		// どうせ玉が入玉してないときはほとんど判定コストゼロだしな
		//  return pos.SengenGachi();
		// ↑この関数は、勝ちならばMOVE_NONE以外が返る。勝ちならMOVE_WINもしくは、勝ちになる指し手が返る。

		return Move::none();
	}

	// 現局面で1手詰めであるかを判定する。1手詰めであればその指し手を返す。
	Move mate_1ply(const Position& pos)
	{
		return pos.side_to_move() == BLACK ? Mate::mate_1ply_imp<BLACK>(pos) : Mate::mate_1ply_imp<WHITE>(pos);
	}

} // namespace Mate

// templateの明示的な実体化
//template Move Mate::mate_1ply_imp<BLACK>(const Position& pos);
//template Move Mate::mate_1ply_imp<WHITE>(const Position& pos);

} // namespace YaneuraOu

// clang-format on

