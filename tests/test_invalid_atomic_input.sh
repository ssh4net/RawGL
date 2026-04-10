#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"

run_case() {
  local case_name="$1"
  shift

  local out_file="$script_dir/outputs/${case_name}.exr"
  local log_file="$script_dir/outputs/${case_name}.log"

  rm -f "$out_file" "$log_file"

  set +e
  "$rawgl_bin" "$@" >"$log_file" 2>&1
  local status=$?
  set -e

  cat "$log_file"

  if [ "$status" -eq 0 ]; then
    echo "${case_name}: expected failure" >&2
    exit 1
  fi

  if [ -f "$out_file" ]; then
    echo "${case_name}: unexpectedly produced an output file" >&2
    exit 1
  fi
}

run_case atomic_too_many_values \
  --verbosity 5 \
  --pass_comp "$script_dir/shaders/atomic_counter.comp" \
  --pass_size 1 1 \
  --pass_workgroupsize 1 1 \
  --atomic cntr counter0 5 6 \
  --out o_out0 "$script_dir/outputs/atomic_too_many_values.exr" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32

rg -q "can only have a single value" "$script_dir/outputs/atomic_too_many_values.log"

run_case atomic_unknown_mode \
  --verbosity 5 \
  --pass_comp "$script_dir/shaders/atomic_counter.comp" \
  --pass_size 1 1 \
  --pass_workgroupsize 1 1 \
  --atomic nope counter0 5 \
  --out o_out0 "$script_dir/outputs/atomic_unknown_mode.exr" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32

rg -q "unknown atomic buffer type" "$script_dir/outputs/atomic_unknown_mode.log"
