#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
oiiotool_bin="${RAWGL_OIIOTOOL:-/mnt/e/UBc/Release/bin/oiiotool}"
out_first="$script_dir/outputs/atomic_counter_reuse_first.exr"
out_second="$script_dir/outputs/atomic_counter_reuse_second.exr"

rm -f "$out_first" "$out_second"

"$rawgl_bin" \
  --verbosity 5 \
  --pass_comp "$script_dir/shaders/atomic_counter.comp" \
  --pass_size 1 1 \
  --pass_workgroupsize 1 1 \
  --atomic cntr counter0 5 \
  --out o_out0 "$out_first" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32 \
  --pass_comp "$script_dir/shaders/atomic_counter.comp" \
  --pass_size 1 1 \
  --pass_workgroupsize 1 1 \
  --out o_out0 "$out_second" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32

test -f "$out_first"
test -f "$out_second"

stats_first="$("$oiiotool_bin" "$out_first" --printstats)"
stats_second="$("$oiiotool_bin" "$out_second" --printstats)"

printf '%s\n' "$stats_first"
printf '%s\n' "$stats_second"

printf '%s\n' "$stats_first" | rg -q "Stats Avg: 5\\.000000 0\\.000000 0\\.000000 1\\.000000"
printf '%s\n' "$stats_second" | rg -q "Stats Avg: 0\\.000000 0\\.000000 0\\.000000 1\\.000000"
