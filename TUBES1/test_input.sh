#!/usr/bin/env bash
# Regression checks for the input boundary; not a simulation implementation.
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
binary=${1:-"$root/tubes1"}
work=$(mktemp -d "${TMPDIR:?TMPDIR must be set}/tubes1-input.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
printf '80\n14 10 24\n20.5\n' > "$work/tubes1.in"
printf 'preserve existing output\n' > "$work/tubes1.out"
cp "$work/tubes1.out" "$work/baseline.out"
if (cd "$work" && "$binary" > stdout.log 2> stderr.log); then
    printf 'FAIL: fractional capacity with missing speed was accepted\n' >&2
    exit 1
fi
cmp "$work/baseline.out" "$work/tubes1.out"
printf 'PASS: fractional capacity with missing speed rejected; output preserved\n'
invalid_inputs=(
    '80\n14 10 24\n20.5 30\n'
    '80\n14 10 24\n9223372036854775808 30\n'
    '80\n14 10 24\n-9223372036854775809 30\n'
    '80\n14 10 24\n2147483648 30\n'
    '80\n14 10 24\n20x 30\n'
    '80\n14 10 24\n2e1 30\n'
    '80\n14 10 24\n20\n'
    '80\n14 10 24\n20 30 extra\n'
    '80\n14 10 24\n20 30x\n'
    '80\n14 10 24\n20 30.5.6\n'
    '80\n14 10 24\n0 30\n'
    '80\n14 10 24\n-1 30\n'
    '80\n14 10 24\n20 0\n'
    '0\n14 10 24\n20 30\n'
    'nan\n14 10 24\n20 30\n'
    '80\ninf 10 24\n20 30\n'
    '80\n-14 10 24\n20 30\n'
    '80\n14 10 24\n20 1e999\n'
    '80\n14 10 24\n20 1e-999\n'
    ''
)
for input in "${invalid_inputs[@]}"; do
    printf '%b' "$input" > "$work/tubes1.in"
    cp "$work/baseline.out" "$work/tubes1.out"
    if (cd "$work" && "$binary" > stdout.log 2> stderr.log); then
        printf 'FAIL: invalid input accepted: %s\n' "$input" >&2
        exit 1
    fi
    cmp "$work/baseline.out" "$work/tubes1.out"
done
# Reject an oversized token rather than splitting it into multiple parameters.
printf '80\n14 10 24\n' > "$work/tubes1.in"
printf '%0130d 30\n' 20 >> "$work/tubes1.in"
if (cd "$work" && "$binary" > stdout.log 2> stderr.log); then
    printf 'FAIL: oversized capacity token accepted\n' >&2
    exit 1
fi
cmp "$work/baseline.out" "$work/tubes1.out"
# Parameter separators may be any whitespace, and final newline is optional.
for input in '80\n14 10 24\n20 30\n' '80\t14\t10\t24\t+20\t30' '8e1 14.0 10.0 24.0 20 3e1  \n'; do
    printf '%b' "$input" > "$work/tubes1.in"
    (cd "$work" && "$binary" > stdout.log 2> stderr.log)
    cmp "$root/tubes1.out" "$work/tubes1.out"
done
printf 'PASS: all input boundary checks and deterministic demo comparisons\n'
