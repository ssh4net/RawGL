#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
out_file="$script_dir/outputs/invalid_bg_color.exr"
log_file="$script_dir/outputs/invalid_bg_color.log"

rm -f "$out_file" "$log_file"

set +e
"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/solid_white.frag" \
  --pass_size 8 8 \
  --bg_color 0.1 nope 0.3 1.0 \
  --out OutSample "$out_file" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32 \
  >"$log_file" 2>&1
status=$?
set -e

cat "$log_file"

if [ "$status" -eq 0 ]; then
  echo "Expected invalid bg_color to fail" >&2
  exit 1
fi

if [ -f "$out_file" ]; then
  echo "Invalid bg_color unexpectedly produced an output file" >&2
  exit 1
fi

rg -q "bg_color: invalid numeric value" "$log_file"
