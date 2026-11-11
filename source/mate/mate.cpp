#include "mate.h"
#include "../position.h"
#include "../movegen.h"

namespace YaneuraOu {
namespace Mate {

// 1手詰めテーブルの初期化関数
// ※　これは、mate1ply_without_effect.cppか、mate1ply_with_effect.cppのいずれかで定義されている。
void init_mate_1ply();

// ---------------------
//     Mate::init()
// ---------------------

// Mate関連で使うテーブルの初期化
void init() { init_mate_1ply(); }

// ---------------------
// weak_mate_3ply()
// ---------------------

// 利きのある場所への取れない近接王手からの3手詰め
Move weak_mate_3ply(const Position& pos, int ply) {
    // 1手詰めであるならこれを返す
    Move m = Mate::mate_1ply(pos);
    if (m)
        return m;

    // 詰まない
    if (ply <= 1)
        return Move::none();

    Color    us      = pos.side_to_move();
    Color    them    = ~us;
    Bitboard around8 = kingEffect(pos.square<KING>(them));

    // const剥がし
    Position* This = ((Position*) &pos);

    StateInfo si;
    StateInfo si2;

    // 近接王手で味方の利きがあり、敵の利きのない場所を探す。
    for (auto em : MoveList<CHECKS>(pos))
    {
        Move m = Move(em);

        // 近接王手で、この指し手による駒の移動先に敵の駒がない。
        Square to = m.to_sq();
        if ((around8 & to)

            // toに利きがあるかどうか。mが移動の指し手の場合、mの元の利きを取り除く必要がある。
            && (m.is_drop()
                  ? pos.effected_to(us, to)
                  : (bool) (pos.attackers_to(us, to, pos.pieces() ^ m.from_sq()) ^ m.from_sq()))

            // 敵玉の利きは必ずtoにあるのでそれを除いた利きがあるかどうか。
            && (pos.attackers_to(them, to, pos.pieces()) ^ pos.square<KING>(them))
        )
        {
            if (!pos.legal(m))
                continue;

            ASSERT_LV3(pos.gives_check(m));

            This->do_move(m, si, true);

            ASSERT_LV3(pos.in_check());

            // この局面ですべてのevasionを試す
            for (auto m2 : MoveList<EVASIONS>(pos))
            {
                if (!pos.legal(m2))
                    continue;

                // この指し手で逆王手になるなら、不詰めとして扱う
                if (pos.gives_check(m2))
                    goto NEXT_CHECK;

                This->do_move(m2, si2, false);

                ASSERT_LV3(!pos.in_check());

                if (!weak_mate_3ply(pos, ply - 2))
                {
                    // 詰んでないので、m2で詰みを逃れている。
                    This->undo_move(m2);
                    goto NEXT_CHECK;
                }

                This->undo_move(m2);
            }

            // すべて詰んだ
            This->undo_move(m);

            // mによって3手で詰む。
            return m;

NEXT_CHECK:;
            This->undo_move(m);
        }
    }
    return Move::none();
}

// 連続王手などの千日手判定を行う。
// plies_from_root : 詰み探索開始局面からの手数
// repetition_ply  : 千日手判定を行う最大手数 (16ぐらいでいいと思う)
// or_node         : 攻め方のnodeであればtrue。受け方のnodeであればfalse。
MateRepetitionState mate_repetition(const Position& pos,
                                    const int       plies_from_root,
                                    const int       repetition_ply,
                                    bool            or_node) {
    // この判定、わりと難しい問題がある。何がどう難しいかは、以下のコードを読めばわかる。

    // ここで千日手に到達している可能性がある。
    // or nodeとand nodeとでわりと対称性があるが、REPETITION_DRAWはいずれも詰まない扱いにしないといけないなど、
    // 完全に対称とも言い切れない。

    // 与えられた局面以降で生じた劣等局面と、与えられた局面以前から生じた劣等局面では意味が異なる。
    // 前者は、それで詰むことはないので不詰扱いして良いが、後者は、一時的に劣等局面に入ったあと詰むことがあるので不詰扱いできない。
    // 例) XX歩打ち(王手)、XX玉(玉を逃げる)、の局面をdfpn_mate()で詰むかを判定する場合、この打った歩を成り捨てて、同玉から詰むことがある。
    // 　　このとき成り捨てた局面は劣等局面であるが、このように詰むことがあるので、不詰扱いしてはならない。

    if (or_node)
    {
        switch (pos.is_repetition(plies_from_root))
        {
        case REPETITION_DRAW :
        case REPETITION_LOSE :
        case REPETITION_INFERIOR :  // 駒損したのでplies_from_root以降で生じているから、これは不詰扱いして良い。
            return MateRepetitionState::Mated;  // 不詰 = 相手の勝ち = Mated

        case REPETITION_WIN :  // 連続王手の千日手で勝ちになった。(or nodeで普通これはないはずだが)
        case REPETITION_SUPERIOR :  // 相手は駒損をしているが、即座に詰みとは言い切れない。
            // しかしこれが詰まないなら、相手は駒損せずに詰みを逃れる手段があるのでそちらを選択するだろうから、これは調べる必要がなく、詰み扱いで良い。
            return MateRepetitionState::Mate;

        case REPETITION_NONE :
            // 再度、普通の深さで千日手になっていないかチェックしないといけない。
            break;

        default :
            UNREACHABLE;
        }

        if (plies_from_root < repetition_ply)
        {
            // 普通の深さで再度千日手のチェックを行う。

            switch (pos.is_repetition(repetition_ply))
            {
            case REPETITION_DRAW :  // 千日手模様で逃れられる。
            case REPETITION_LOSE :
                return MateRepetitionState::Mated;  // 詰まない

            case REPETITION_WIN :  // 連続王手の千日手で勝ちになった。(or nodeで普通これはないはずだが、呼び出し元以前の局面によってはありうる)
                return MateRepetitionState::Mate;  // 詰み

            case REPETITION_NONE :
            case REPETITION_INFERIOR :  // 駒損はしたが、それはroot以前の局面から見ると、であって、歩を成り捨てて劣等局面には突入するが、詰むようなケースかもしれない。
            case REPETITION_SUPERIOR :  // 相手は駒損をしているが、即座に詰みとは言い切れない。
                break;

            default :
                UNREACHABLE;
            }
        }
    }
    else
    {
        // and node

        switch (pos.is_repetition(plies_from_root))
        {
        case REPETITION_DRAW :  // 詰まない(千日手で逃れている)
        case REPETITION_WIN :   // 連続王手の千日手で勝ちになった。
        case REPETITION_SUPERIOR: // 駒得をしている。即座に詰まないとは言い切れないが、これでこのあと詰まされるとしたら相手は駒損せずに詰ますことができるのでこの指し手は不詰扱いして良い。
            return MateRepetitionState::Mate;  // 不詰 = and node側から見ると勝ち

        case REPETITION_LOSE :  // 連続王手の千日手による反則負け。
        case REPETITION_INFERIOR: // 駒損したものの即座に詰まないとは言い切れないが、これで詰まないとしたら駒損しないほうの変化でも詰まないので、これは詰み扱いして良い。
            return MateRepetitionState::Mated;  // 詰み = and node側から見ると負け

        case REPETITION_NONE :
            // 再度、普通の深さで千日手になっていないかチェックしないといけない。
            break;

        default :
            UNREACHABLE;
        }

        if (plies_from_root < repetition_ply)
        {
            // 普通の深さ(16手)で再度千日手のチェックを行う。
            switch (pos.is_repetition(repetition_ply))
            {
            case REPETITION_DRAW :
            case REPETITION_WIN :                  // 連続王手の千日手で勝ちになった。
                return MateRepetitionState::Mate;  // 勝ち

            case REPETITION_LOSE :
                return MateRepetitionState::Mated;  // 負け

            case REPETITION_NONE :
            case REPETITION_INFERIOR: // 駒損はしたが、今回は詰むとは言えない。
            case REPETITION_SUPERIOR: // 攻め方が駒損をしているが、即座にこの筋がないとは言い切れない。(MCTSの探索ならそんな指し手はしないかも知れないが)
                break;

            default :
                UNREACHABLE;
            }
        }
    }
    return MateRepetitionState::Unknown;
}


}  // namespace Mate
} // namespace YaneuraOu


