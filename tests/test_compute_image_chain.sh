#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
oiiotool_bin="${RAWGL_OIIOTOOL:-/mnt/e/UBc/Release/bin/oiiotool}"
out_file="$script_dir/outputs/compute_image_chain.exr"

rm -f "$out_file"

"$rawgl_bin" \
  --verbosity 5 \
  --pass_comp "$script_dir/shaders/image_chain_source.comp" \
  --pass_size 1 1 \
  --pass_workgroupsize 1 1 \
  --pass_comp "$script_dir/shaders/image_chain_consume.comp" \
  --pass_size 1 1 \
  --pass_workgroupsize 1 1 \
  --in u_mid0 o_mid0::0 \
  --out o_out0 "$out_file" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32

test -f "$out_file"

stats="$("$oiiotool_bin" "$out_file" --printstats)"
printf '%s\n' "$stats"

printf '%s\n' "$stats" | rg -q "Stats Avg: 0\\.250000 0\\.500000 0\\.750000 1\\.000000"
