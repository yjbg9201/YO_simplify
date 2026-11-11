#!/usr/bin/env bash
# 探索が変わっていないことを確認する回帰テスト。
#
#   tools/check_bench.sh <エンジンの実行ファイル> [実行用ラッパー...]
#
# ダミーの評価関数(tools/nnue_dummy_gen.pyが固定seedで生成する)を使って
# "bench"を固定depth・1スレッドで実行し、総探索ノード数が期待値と一致するかを調べる。
# 探索・評価関数・指し手生成のいずれかの挙動が変わるとノード数が変わる。
# CPUやコンパイラが違ってもノード数は同じになる。
set -eu

EXPECTED_NODES=968144   # bench 64 1 12 default depth

engine=$1; shift
dir=$(cd "$(dirname "$engine")" && pwd)
root=$(cd "$(dirname "$0")/.." && pwd)
python=$(command -v python3 || command -v python)

mkdir -p "$dir/eval"
if [ ! -f "$dir/eval/nn.bin" ]; then
	"$python" "$root/tools/nnue_dummy_gen.py" halfkp_256x2-32-32 "$dir/eval/nn.bin" > /dev/null
fi

out=$(printf 'bench 64 1 12 default depth\nquit\n' | "$@" "$dir/$(basename "$engine")" 2>&1)
nodes=$(printf '%s\n' "$out" | tr -d '\r' | awk '/^Nodes searched/ {print $4}')
echo "$out" | tail -n 4

if [ "$nodes" != "$EXPECTED_NODES" ]; then
	echo "NG: nodes = ${nodes:-<none>}, expected = $EXPECTED_NODES"
	exit 1
fi
echo "OK: nodes = $nodes"
