#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
out_file="$script_dir/outputs/invalid_numeric_input.exr"
log_file="$script_dir/outputs/invalid_numeric_input.log"

rm -f "$out_file" "$log_file"

set +e
"$rawgl_bin" \
  --verbosity 5 \
  --pass_comp "$script_dir/shaders/vec2_uniform.comp" \
  --pass_size 1 1 \
  --pass_workgroupsize 1 1 \
  --in u_value 1 \
  --out o_out0 "$out_file" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32 \
  >"$log_file" 2>&1
status=$?
set -e

cat "$log_file"

if [ "$status" -eq 0 ]; then
  echo "Expected invalid numeric input to fail" >&2
  exit 1
fi

if [ -f "$out_file" ]; then
  echo "Invalid numeric input unexpectedly produced an output file" >&2
  exit 1
fi

rg -q "missing numeric values|invalid numeric value" "$log_file"
