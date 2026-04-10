#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
out_file="$script_dir/outputs/missing_input_uniform.exr"
log_file="$script_dir/outputs/missing_input_uniform.log"

rm -f "$out_file" "$log_file"

set +e
"$rawgl_bin" \
  --verbosity 5 \
  --pass_comp "$script_dir/shaders/vec2_uniform.comp" \
  --pass_size 1 1 \
  --pass_workgroupsize 1 1 \
  --in u_missing 1 2 \
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
  echo "Expected missing input uniform to fail" >&2
  exit 1
fi

if [ -f "$out_file" ]; then
  echo "Missing input uniform unexpectedly produced an output file" >&2
  exit 1
fi

rg -q "program uniform not found" "$log_file"
