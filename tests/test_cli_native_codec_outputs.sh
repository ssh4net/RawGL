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

jpeg_out="$script_dir/outputs/cli_native_codec.jpg"
png_out="$script_dir/outputs/cli_native_codec.png"
tiff_out="$script_dir/outputs/cli_native_codec.tif"
exr_out="$script_dir/outputs/cli_native_codec.exr"
jp2_out="$script_dir/outputs/cli_native_codec.jp2"

rm -f "$jpeg_out" "$png_out" "$tiff_out" "$exr_out" "$jp2_out"

"$rawgl_bin" \
  --verbosity 5 \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/pass1.frag" \
  --pass_size 32 32 \
  --in InSample "$script_dir/inputs/sky.jpg" \
  --in_backend native_only \
  --in_jpeg_color_transform rgb \
  --out OutSample "$jpeg_out" \
  --out_format rgb32f \
  --out_channels 3 \
  --out_bits 8 \
  --out_jpeg_quality 93 \
  --out_jpeg_progressive true \
  --out_jpeg_optimize true \
  --out_jpeg_subsampling 444 \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/pass1.frag" \
  --pass_size 32 32 \
  --in InSample "$script_dir/inputs/sky.jpg" \
  --in_backend native_only \
  --in_jpeg_color_transform rgb \
  --out OutSample "$png_out" \
  --out_format rgb32f \
  --out_channels 3 \
  --out_bits 16 \
  --out_png_compression 1 \
  --out_png_interlace false \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/pass1.frag" \
  --pass_size 32 32 \
  --in InSample "$script_dir/inputs/sky.jpg" \
  --in_backend native_only \
  --in_jpeg_color_transform rgb \
  --out OutSample "$tiff_out" \
  --out_format rgb32f \
  --out_channels 3 \
  --out_bits 16 \
  --out_tiff_layout tiled \
  --out_tiff_tile_size 16 16 \
  --out_tiff_compression deflate \
  --out_tiff_predictor horizontal \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/pass1.frag" \
  --pass_size 32 32 \
  --in InSample "$script_dir/inputs/sky.jpg" \
  --in_backend native_only \
  --in_jpeg_color_transform rgb \
  --out OutSample "$exr_out" \
  --out_format rgb32f \
  --out_channels 3 \
  --out_bits 16 \
  --out_exr_layout tiled \
  --out_exr_tile_size 16 16 \
  --out_exr_compression zip \
  --out_exr_line_order increasing_y \
  --pass_vertfrag "$script_dir/shaders/empty.vert" "$script_dir/shaders/pass1.frag" \
  --pass_size 32 32 \
  --in InSample "$script_dir/inputs/sky.jpg" \
  --in_backend native_only \
  --in_jpeg_color_transform rgb \
  --out OutSample "$jp2_out" \
  --out_format rgb32f \
  --out_channels 3 \
  --out_bits 16 \
  --out_jpeg2000_lossless true

test -f "$jpeg_out"
test -f "$png_out"
test -f "$tiff_out"
test -f "$exr_out"
test -f "$jp2_out"

"$oiiotool_bin" "$jpeg_out" --info >/dev/null
"$oiiotool_bin" "$png_out" --info >/dev/null
"$oiiotool_bin" "$tiff_out" --info >/dev/null
"$oiiotool_bin" "$exr_out" --info >/dev/null
"$oiiotool_bin" "$jp2_out" --info >/dev/null
