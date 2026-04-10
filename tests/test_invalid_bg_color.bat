@echo off
setlocal

set RAWGL_BIN=%~dp0..\bin\RawGL\RawGL.exe
if not "%1"=="" set RAWGL_BIN=%~1

set OUT_FILE=%~dp0outputs\invalid_bg_color.exr
set LOG_FILE=%~dp0outputs\invalid_bg_color.log

if exist "%OUT_FILE%" del /f /q "%OUT_FILE%"
if exist "%LOG_FILE%" del /f /q "%LOG_FILE%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%~dp0shaders\empty.vert" "%~dp0shaders\solid_white.frag" ^
  --pass_size 8 8 ^
  --bg_color 0.1 nope 0.3 1.0 ^
  --out OutSample "%OUT_FILE%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32 >"%LOG_FILE%" 2>&1

if errorlevel 1 goto :ok
type "%LOG_FILE%"
echo Expected invalid bg_color to fail
exit /b 1

:ok
type "%LOG_FILE%"
if exist "%OUT_FILE%" exit /b 1
exit /b 0
