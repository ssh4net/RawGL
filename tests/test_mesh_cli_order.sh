#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
out_file="$script_dir/outputs/mesh_cli_order.exr"
log_file="$script_dir/outputs/mesh_cli_order.log"

rm -f "$out_file" "$log_file"

set +e
"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/solid_white.frag" \
  --pass_size 8 8 \
  --pass_mesh mesh tris true rend tr "$script_dir/inputs/nonexistent_mesh_order.ply" \
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
  echo "Expected missing mesh file to fail" >&2
  exit 1
fi

if [ -f "$out_file" ]; then
  echo "Missing mesh file unexpectedly produced an output file" >&2
  exit 1
fi

if rg -q "Only PLY and OBJ meshes supported|mesh file path not found|mesh attributes must be key/value pairs|unknown mesh parameter" "$log_file"; then
  echo "Mesh CLI order was rejected by the parser instead of reaching mesh loading" >&2
  exit 1
fi

rg -q "Failed to load mesh" "$log_file"
