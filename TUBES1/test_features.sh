#!/usr/bin/env bash
# Tests the real C executable; does not implement the simulation.
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binary=${1:-"$root/tubes1"}
work=$(mktemp -d "${TMPDIR:?}/tubes1-features.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
cp "$root/tubes1.in" "$work/custom.in"
(cd "$work" && "$binary" custom.in custom.out)
test -s "$work/custom.out" || { printf 'FAIL: CLI custom input/output not honored\n' >&2; exit 1; }
test ! -e "$work/tubes1.out"
printf 'PASS: custom CLI paths\n'
grep -q 'Time unit: minute' "$work/custom.out"
grep -q 'Avg delay (min)' "$work/custom.out"
grep -q 'Simulation ended at 4800.0000 min' "$work/custom.out"
printf 'PASS: time statistics displayed in minutes\n'
grep -Fq '[PASS] Conservation: arrivals=3778, completed=3742' "$work/custom.out"
grep -Fq '[PASS] Capacity' "$work/custom.out"
grep -Fq '[PASS] State invariants' "$work/custom.out"
grep -Fq '[PASS] Minimum loop' "$work/custom.out"
grep -Fq '[PASS] Minimum stop' "$work/custom.out"
grep -Fq '[PASS] Simulation horizon' "$work/custom.out"
grep -Fq 'All internal checks passed (not statistical validation).' "$work/custom.out"
printf 'PASS: verification summary\n'
cp "$work/custom.in" "$work/input-baseline"
ln -s custom.in "$work/alias.in"
ln "$work/custom.in" "$work/hardlink.in"
for output in custom.in alias.in hardlink.in; do
    if (cd "$work" && "$binary" custom.in "$output" > stdout.log 2> stderr.log); then
        printf 'FAIL: input/output alias accepted\n' >&2; exit 1
    fi
    cmp "$work/custom.in" "$work/input-baseline"
done
printf 'sentinel\n' > "$work/preserve.out"
cp "$work/preserve.out" "$work/output-baseline"
printf '80\n14 10 24\n20.5\n' > "$work/bad.in"
for input in missing.in bad.in; do
    if (cd "$work" && "$binary" "$input" preserve.out > stdout.log 2> stderr.log); then
        printf 'FAIL: invalid CLI input accepted\n' >&2; exit 1
    fi
    cmp "$work/preserve.out" "$work/output-baseline"
done
if (cd "$work" && "$binary" custom.in missing-dir/result.out > stdout.log 2> stderr.log); then
    printf 'FAIL: unavailable output path accepted\n' >&2; exit 1
fi
if (cd "$work" && "$binary" custom.in preserve.out extra > stdout.log 2> stderr.log); then
    printf 'FAIL: excess arguments accepted\n' >&2; exit 1
fi
cmp "$work/preserve.out" "$work/output-baseline"
printf '0.001\n14 10 24\n20 30\n' > "$work/short.in"
(cd "$work" && "$binary" short.in short.out)
grep -Fq '[SKIP] Minimum loop: no completed loop' "$work/short.out"
grep -Fq '[SKIP] Minimum stop: no completed stop' "$work/short.out"
if grep -Eq 'nan|1000000000000|\[FAIL\]' "$work/short.out"; then
    printf 'FAIL: short-run statistics/verification invalid\n' >&2; exit 1
fi
printf '0.151\n14 10 24\n20 30\n' > "$work/boarding.in"
(cd "$work" && "$binary" boarding.in boarding.out)
grep -q 'boarding=1' "$work/boarding.out"
grep -Fq '[PASS] Conservation' "$work/boarding.out"
printf '10\n0.0001 0.0001 0.0001\n20 60\n' > "$work/fast.in"
(cd "$work" && "$binary" fast.in fast.out)
grep -Fq 'Minimum loop: 25.0000 min; bound 25.0000 min' "$work/fast.out"
printf 'PASS: CLI failures, aliases, short run, boarding, parameter-derived bound\n'
