#!/usr/bin/env bash
# Regression / correctness tests for the Obfuscator pass plugin.
#
# Strategy: differential testing. Compile a program normally and through the
# passes, run both, and compare output -- identical output means the
# transforms preserved semantics. Then confirm the obfuscation actually
# happened (strings encrypted, IR verifies).
#
# Usage:
#   tests/run.sh [path-to-MyPass.so] [llvm-tool-suffix]
# Examples:
#   tests/run.sh                      # uses build/MyPass.so, `clang`/`opt`
#   tests/run.sh build/MyPass.so -21  # uses clang-21 / opt-21 (match your build!)
set -uo pipefail

SO="${1:-build/MyPass.so}"
SUF="${2:-}"
CLANG="clang${SUF}"
OPT="opt${SUF}"
DIR="$(cd "$(dirname "$0")" && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

if [ ! -f "$SO" ]; then echo "plugin not found: $SO (build first, or pass its path)"; exit 2; fi

pass=0; fail=0
check(){ if [ "$1" = "$2" ]; then echo "  PASS: $3"; pass=$((pass+1));
         else echo "  FAIL: $3 (got '$1' want '$2')"; fail=$((fail+1)); fi; }

$CLANG -O0 -emit-llvm -S "$DIR/diff_test.c" -o "$tmp/dt.ll"
BASE="$($CLANG "$tmp/dt.ll" -o "$tmp/base" && "$tmp/base")"
echo "baseline: $BASE"

# 1. Each pass (and the full pipeline) must verify and preserve behaviour.
for spec in "string-encrypt" "instr-subst" "bogus-cf" \
            "string-encrypt,function(instr-subst,bogus-cf)"; do
  if ! $OPT -load-pass-plugin="$SO" -passes="$spec,verify" "$tmp/dt.ll" -S -o "$tmp/o.ll" 2>"$tmp/v.log"; then
    echo "  FAIL: [$spec] opt/verify errored"; cat "$tmp/v.log"; fail=$((fail+1)); continue
  fi
  check "$($CLANG "$tmp/o.ll" -o "$tmp/o" && "$tmp/o")" "$BASE" "[$spec]"
done

# 2. CLI knob reaches the pass (and still correct at 100%).
$OPT -load-pass-plugin="$SO" -passes='bogus-cf,verify' -obfuscation-probability=100 \
     "$tmp/dt.ll" -S -o "$tmp/o.ll" 2>/dev/null
check "$($CLANG "$tmp/o.ll" -o "$tmp/o" && "$tmp/o")" "$BASE" "bogus-cf@100"

# 3. Realistic path: clang -fpass-plugin at -O0 and -O2.
for O in O0 O2; do
  check "$($CLANG -$O -fpass-plugin="$SO" "$DIR/diff_test.c" -o "$tmp/fp" && "$tmp/fp")" \
        "$BASE" "clang -$O -fpass-plugin"
done

# 4. String encryption actually removed the plaintext.
$CLANG -O2 -fpass-plugin="$SO" "$DIR/test.c" -o "$tmp/se"
if strings "$tmp/se" | grep -qE "hello, obfuscated|SECRET_TOKEN"; then
  echo "  FAIL: plaintext still present in binary"; fail=$((fail+1))
else
  echo "  PASS: strings encrypted (no plaintext in binary)"; pass=$((pass+1))
fi

# 5. Deterministic output across runs (seeded from function name).
$OPT -load-pass-plugin="$SO" -passes=instr-subst "$tmp/dt.ll" -S -o "$tmp/d1.ll" 2>/dev/null
$OPT -load-pass-plugin="$SO" -passes=instr-subst "$tmp/dt.ll" -S -o "$tmp/d2.ll" 2>/dev/null
if diff -q <(grep -v ModuleID "$tmp/d1.ll") <(grep -v ModuleID "$tmp/d2.ll") >/dev/null; then
  echo "  PASS: deterministic output"; pass=$((pass+1))
else
  echo "  FAIL: non-deterministic output"; fail=$((fail+1))
fi

echo
echo "RESULT: $pass passed, $fail failed"
[ "$fail" -eq 0 ]