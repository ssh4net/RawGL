@echo off
setlocal

set RAWGL_BIN=%~dp0..\bin\RawGL\RawGL.exe
if not "%1"=="" set RAWGL_BIN=%~1

set OUT_DEFAULT=%~dp0outputs\cull_default.exr
set OUT_FRONT=%~dp0outputs\cull_front.exr
set OUT_DISABLED=%~dp0outputs\cull_disabled.exr

if exist "%OUT_DEFAULT%" del /f /q "%OUT_DEFAULT%"
if exist "%OUT_FRONT%" del /f /q "%OUT_FRONT%"
if exist "%OUT_DISABLED%" del /f /q "%OUT_DISABLED%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%~dp0shaders\empty.vert" "%~dp0shaders\solid_white.frag" ^
  --pass_size 8 8 ^
  --out OutSample "%OUT_DEFAULT%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32
if errorlevel 1 exit /b 1

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%~dp0shaders\empty.vert" "%~dp0shaders\solid_white.frag" ^
  --pass_size 8 8 ^
  --cull face fr ^
  --out OutSample "%OUT_FRONT%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32
if errorlevel 1 exit /b 1

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%~dp0shaders\empty.vert" "%~dp0shaders\solid_white.frag" ^
  --pass_size 8 8 ^
  --cull face fr enable false ^
  --out OutSample "%OUT_DISABLED%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32
if errorlevel 1 exit /b 1

if not exist "%OUT_DEFAULT%" exit /b 1
if not exist "%OUT_FRONT%" exit /b 1
if not exist "%OUT_DISABLED%" exit /b 1
