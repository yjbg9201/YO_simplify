# YO_simplify

[やねうら王 (YaneuraOu)](https://github.com/yaneurao/YaneuraOu) から、**対局と検討に必要な機能だけ**を残した簡素版です。

探索部と評価関数のコードパスには手を入れておらず、同じ評価関数ファイルを使えば元のやねうら王(NNUE版)と同じ手を同じノード数で読みます。削ったのは「使わない機能」と「使われていないコード」だけです。

- 探索部 : やねうら王の通常探索部 (V9.80git 時点)
- 評価関数 : 標準NNUE (HalfKP 256x2-32-32) のみ
- 対応環境 : Linux / Windows / macOS の x64 と ARM64

## 残した機能

| 区分 | 内容 |
| --- | --- |
| 対局 | USIプロトコル、持ち時間各種(秒読み・フィッシャー)、Ponder / Stochastic Ponder、入玉宣言、千日手、定跡の読み出し(やねうら王形式 `.db` / `.ybb`、on the fly対応) |
| 検討 | MultiPV、`ConsiderationMode`、`go infinite`、`searchmoves`、depth / nodes 制限 |
| 性能 | SSE2〜AVX-512 / NEON の各SIMD実装、NUMA対応、Large Page、LTO / PGOビルド |
| 補助コマンド | `bench`、`d`(局面表示)、`eval`、`moves`、`getoption`、`log`、`go perft` |

## 削除したもの

- ふかうら王(dlshogi互換エンジン)、KPPT / KPP_KKPT / 駒得評価関数、詰将棋エンジン、ユーザーエンジンの雛形
- 標準型以外のNNUE / SFNNアーキテクチャ、評価関数の実行ファイルへの埋め込み
- 定跡の生成・編集(`makebook`)、PolicyBook、Apery形式の定跡
- `go mate`(df-pn詰将棋ルーチン)、テストコマンド、探索パラメーターの自動調整、`qsearch_psv` などの開発用コマンド
- Stockfishとの比較用に残されていた `#if STOCKFISH` ブロック、`#if 0` の死にコード
- WebAssembly / Android / 32bit 向けのコード、Docker・各種ビルドスクリプト

ソースコードは約73,000行から約36,000行になりました。

## ビルド

### Linux / macOS / MSYS2 (clang++ または g++)

```bash
cd source
make -j8 normal TARGET_CPU=AVX2 COMPILER=clang++
```

`source/YaneuraOu-by-gcc` (Windowsでは `.exe`) ができます。

| `TARGET_CPU` | 対象 |
| --- | --- |
| `AVX2` | Haswell以降のIntel、Zen3以降のAMD(`ZEN3`も可) |
| `ZEN1` / `ZEN2` | 初期のRyzen (BMI2を使わない) |
| `AVX512` / `AVX512VNNI` / `AVXVNNI` | 対応CPU向け |
| `SSE42` / `SSE41` / `SSSE3` / `SSE2` | 古いx64 CPU |
| `ARMV8` / `ARMV8_DOTPROD` / `GRAVITON2` / `WINARM64` | ARM64 |
| `APPLEM1` / `APPLEAVX2` / `APPLESSE42` | macOS |

その他のターゲット:

```bash
make tournament          # 大会用。千日手検出を簡略化する(検討には不向き)
make pgo COMPILER=g++ EVAL_DIR=/path/to/eval   # PGOビルド
make clean
```

> `ARMV8_DOTPROD` は clang++ でビルドしてください(g++ではNEONの型変換でエラーになります。これは元のやねうら王と同じです)。

### Visual Studio 2022

`source/YaneuraOu.sln` を開き、`Release`(AVX2) または `Release-SSE42` をビルドします。`build/` に実行ファイルができます。

## 使い方

実行ファイルと同じフォルダに評価関数と定跡を置きます。

```text
YaneuraOu-by-gcc
eval/nn.bin               標準NNUE型(HalfKP 256x2-32-32)の評価関数ファイル
book/standard_book.db     定跡(任意。無ければ BookFile を no_book に)
```

あとはUSI対応のGUI(将棋所、ShogiGUI、ShogiHomeなど)にエンジンとして登録するだけです。エンジンオプションはやねうら王と同じ名前・同じ意味です。

## 動作の確認方法

探索が元のやねうら王から変わっていないことは、`bench` の総探索ノード数で確認しています。

```bash
tools/check_bench.sh source/YaneuraOu-by-gcc
```

固定seedのダミー評価関数で `bench` を1スレッド・固定depthで実行し、ノード数が期待値と一致するかを調べます。探索・評価関数・指し手生成のどれかの挙動が変わればノード数が変わります。期待値は簡素化前のやねうら王で測った値で、CPUやコンパイラが違っても同じ値になります。CI(GitHub Actions)でも各環境でこれを実行しています。

簡素化の各コミットは、この値が変わらないことを確認しながら積んであります。

## ライセンス

やねうら王と同じく GPLv3 です。[LICENSE](LICENSE) を参照してください。
探索部・評価関数を含むすべてのコードの著作権は、やねうら王およびその元になったStockfish等の各プロジェクトの作者に帰属します。
