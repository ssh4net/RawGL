#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
rawgl_bin="${RAWGL_BIN:-$repo_root/build_linux_release/RawGL}"
oiiotool_bin="${RAWGL_OIIOTOOL:-/mnt/e/UBc/Release/bin/oiiotool}"
out_file="$script_dir/outputs/mesh_ao_sponge.exr"

rm -f "$out_file"

"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/mesh_ao.vert" "$script_dir/shaders/mesh_ao.frag" \
  --pass_size 256 256 \
  --bg_color 0 0 0 1 \
  --pass_mesh mesh tris true rend tr "$script_dir/inputs/sponge.ply" \
  --out OutSample "$out_file" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32

test -f "$out_file"

stats="$("$oiiotool_bin" "$out_file" --printstats)"
printf '%s\n' "$stats"
printf '%s\n' "$stats" | awk '
/Stats Min:/ {
    if ($3 != "0.000000" || $4 != "0.000000" || $5 != "0.000000" || $6 != "1.000000") {
        exit 1
    }
}
/Stats Max:/ {
    if ($3 < 0.95 || $4 < 0.95 || $5 < 0.95 || $6 != "1.000000") {
        exit 1
    }
}
/Stats Avg:/ {
    if ($3 < 0.75 || $3 > 0.82 || $4 < 0.75 || $4 > 0.82 || $5 < 0.75 || $5 > 0.82 || $6 != "1.000000") {
        exit 1
    }
}
'
