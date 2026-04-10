#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
log_file="$script_dir/outputs/invalid_vertfrag_arity.log"
out_file="$script_dir/outputs/invalid_vertfrag_arity.exr"

rm -f "$log_file" "$out_file"

set +e
"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/pass1.frag" "$script_dir/shaders/pass2.frag" \
  --pass_size 8 8 \
  --out OutSample "$out_file" \
  >"$log_file" 2>&1
status=$?
set -e

test "$status" -ne 0
test ! -f "$out_file"
rg -q "pass_vertfrag: must have one combined shader file or two stage files" "$log_file"
