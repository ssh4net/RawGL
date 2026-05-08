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

out_default="$script_dir/outputs/cull_default.exr"
out_front="$script_dir/outputs/cull_front.exr"
out_disabled="$script_dir/outputs/cull_disabled.exr"

rm -f "$out_default" "$out_front" "$out_disabled"

"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/solid_white.frag" \
  --pass_size 8 8 \
  --out OutSample "$out_default" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32

"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/solid_white.frag" \
  --pass_size 8 8 \
  --cull face fr \
  --out OutSample "$out_front" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32

"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/solid_white.frag" \
  --pass_size 8 8 \
  --cull face fr enable false \
  --out OutSample "$out_disabled" \
  --out_format rgba32f \
  --out_channels 4 \
  --out_alpha_channel 3 \
  --out_bits 32

test -f "$out_default"
test -f "$out_front"
test -f "$out_disabled"

stats_default="$("$oiiotool_bin" "$out_default" --printstats)"
stats_front="$("$oiiotool_bin" "$out_front" --printstats)"
stats_disabled="$("$oiiotool_bin" "$out_disabled" --printstats)"

printf '%s\n' "$stats_default"
printf '%s\n' "$stats_front"
printf '%s\n' "$stats_disabled"

printf '%s\n' "$stats_default" | rg -q "Stats Avg: 1\\.000000 1\\.000000 1\\.000000 1\\.000000"
printf '%s\n' "$stats_front" | rg -q "Stats Avg: 0\\.000000 0\\.000000 0\\.000000 0\\.000000"
printf '%s\n' "$stats_disabled" | rg -q "Stats Avg: 1\\.000000 1\\.000000 1\\.000000 1\\.000000"
