#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
oiiotool_bin="${RAWGL_OIIOTOOL:-$(command -v oiiotool || true)}"
if [[ -z "$oiiotool_bin" ]]; then
  echo "oiiotool not found; set RAWGL_OIIOTOOL or add oiiotool to PATH." >&2
  exit 1
fi
out_file="$script_dir/outputs/atomic_counter.exr"

rm -f "$out_file"

"$rawgl_bin" \
  --verbosity 5 \
  --pass_comp "$script_dir/shaders/atomic_counter.comp" \
  --pass_size 1 1 \
  --pass_workgroupsize 1 1 \
  --atomic cntr counter0 5 \
  --out o_out0 "$out_file" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 16

test -f "$out_file"

stats="$("$oiiotool_bin" "$out_file" --printstats)"
printf '%s\n' "$stats"
printf '%s\n' "$stats" | rg -q "Stats Avg: 5\\.000000 0\\.000000 0\\.000000 1\\.000000"
