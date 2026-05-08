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
out_file="$script_dir/outputs/frag_pass.exr"

rm -f "$out_file"

"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/single_file_vertfrag.glsl" \
  --pass_size 8 8 \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/pass2.frag" \
  --pass_size 8 8 \
  --in InSample2 OutSample::0 \
  --out OutSample2 "$out_file" \
  --out_format rgb32f \
  --out_channels 3 \
  --out_bits 32

test -f "$out_file"

stats="$("$oiiotool_bin" "$out_file" --printstats)"
printf '%s\n' "$stats"
printf '%s\n' "$stats" | rg -q "Stats Avg: 1\\.000000 0\\.000000 0\\.500000"
