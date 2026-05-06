@echo off
setlocal

set SCRIPT_DIR=%~dp0
set RAWGL_BIN=%SCRIPT_DIR%..\bin\RawGL\RawGL.exe
if not "%RAWGL_BIN_OVERRIDE%"=="" set RAWGL_BIN=%RAWGL_BIN_OVERRIDE%

set JPEG_OUT=%SCRIPT_DIR%outputs\cli_native_codec.jpg
set PNG_OUT=%SCRIPT_DIR%outputs\cli_native_codec.png
set TIFF_OUT=%SCRIPT_DIR%outputs\cli_native_codec.tif
set EXR_OUT=%SCRIPT_DIR%outputs\cli_native_codec.exr
set JP2_OUT=%SCRIPT_DIR%outputs\cli_native_codec.jp2

if exist "%JPEG_OUT%" del /f /q "%JPEG_OUT%"
if exist "%PNG_OUT%" del /f /q "%PNG_OUT%"
if exist "%TIFF_OUT%" del /f /q "%TIFF_OUT%"
if exist "%EXR_OUT%" del /f /q "%EXR_OUT%"
if exist "%JP2_OUT%" del /f /q "%JP2_OUT%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%SCRIPT_DIR%shaders\empty.vert" "%SCRIPT_DIR%shaders\pass1.frag" ^
  --pass_size 32 32 ^
  --in InSample "%SCRIPT_DIR%inputs\sky.jpg" ^
  --in_backend native_only ^
  --in_jpeg_color_transform rgb ^
  --out OutSample "%JPEG_OUT%" ^
  --out_format rgb32f ^
  --out_channels 3 ^
  --out_bits 8 ^
  --out_jpeg_quality 93 ^
  --out_jpeg_progressive true ^
  --out_jpeg_optimize true ^
  --out_jpeg_subsampling 444 ^
  --pass_vertfrag "%SCRIPT_DIR%shaders\empty.vert" "%SCRIPT_DIR%shaders\pass1.frag" ^
  --pass_size 32 32 ^
  --in InSample "%SCRIPT_DIR%inputs\sky.jpg" ^
  --in_backend native_only ^
  --in_jpeg_color_transform rgb ^
  --out OutSample "%PNG_OUT%" ^
  --out_format rgb32f ^
  --out_channels 3 ^
  --out_bits 16 ^
  --out_png_compression 1 ^
  --out_png_interlace false ^
  --pass_vertfrag "%SCRIPT_DIR%shaders\empty.vert" "%SCRIPT_DIR%shaders\pass1.frag" ^
  --pass_size 32 32 ^
  --in InSample "%SCRIPT_DIR%inputs\sky.jpg" ^
  --in_backend native_only ^
  --in_jpeg_color_transform rgb ^
  --out OutSample "%TIFF_OUT%" ^
  --out_format rgb32f ^
  --out_channels 3 ^
  --out_bits 16 ^
  --out_tiff_layout tiled ^
  --out_tiff_tile_size 16 16 ^
  --out_tiff_compression deflate ^
  --out_tiff_predictor horizontal ^
  --pass_vertfrag "%SCRIPT_DIR%shaders\empty.vert" "%SCRIPT_DIR%shaders\pass1.frag" ^
  --pass_size 32 32 ^
  --in InSample "%SCRIPT_DIR%inputs\sky.jpg" ^
  --in_backend native_only ^
  --in_jpeg_color_transform rgb ^
  --out OutSample "%EXR_OUT%" ^
  --out_format rgb32f ^
  --out_channels 3 ^
  --out_bits 16 ^
  --out_exr_layout tiled ^
  --out_exr_tile_size 16 16 ^
  --out_exr_compression zip ^
  --out_exr_line_order increasing_y ^
  --pass_vertfrag "%SCRIPT_DIR%shaders\empty.vert" "%SCRIPT_DIR%shaders\pass1.frag" ^
  --pass_size 32 32 ^
  --in InSample "%SCRIPT_DIR%inputs\sky.jpg" ^
  --in_backend native_only ^
  --in_jpeg_color_transform rgb ^
  --out OutSample "%JP2_OUT%" ^
  --out_format rgb32f ^
  --out_channels 3 ^
  --out_bits 16 ^
  --out_jpeg2000_lossless true

if errorlevel 1 exit /b 1
if not exist "%JPEG_OUT%" exit /b 1
if not exist "%PNG_OUT%" exit /b 1
if not exist "%TIFF_OUT%" exit /b 1
if not exist "%EXR_OUT%" exit /b 1
if not exist "%JP2_OUT%" exit /b 1

endlocal
