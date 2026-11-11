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

}  // namespace Mate
} // namespace YaneuraOu


