#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
oiiotool_bin="${RAWGL_OIIOTOOL:-/mnt/e/UBc/Release/bin/oiiotool}"
out_file="$script_dir/outputs/single_file_vertfrag.exr"

rm -f "$out_file"

"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/single_file_vertfrag.glsl" \
  --pass_size 8 8 \
  --out OutSample "$out_file" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32

test -f "$out_file"

stats="$("$oiiotool_bin" "$out_file" --printstats)"
printf '%s\n' "$stats"
printf '%s\n' "$stats" | rg -q "Stats Avg: 1\\.000000 0\\.250000 0\\.500000 1\\.000000"
