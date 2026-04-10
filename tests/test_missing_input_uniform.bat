@echo off
setlocal

set RAWGL_BIN=%~dp0..\bin\RawGL\RawGL.exe
if not "%1"=="" set RAWGL_BIN=%~1

set OUT_FILE=%~dp0outputs\missing_input_uniform.exr
set LOG_FILE=%~dp0outputs\missing_input_uniform.log

if exist "%OUT_FILE%" del /f /q "%OUT_FILE%"
if exist "%LOG_FILE%" del /f /q "%LOG_FILE%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_comp "%~dp0shaders\vec2_uniform.comp" ^
  --pass_size 1 1 ^
  --pass_workgroupsize 1 1 ^
  --in u_missing 1 2 ^
  --out o_out0 "%OUT_FILE%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32 >"%LOG_FILE%" 2>&1

if errorlevel 1 goto :ok
type "%LOG_FILE%"
echo Expected missing input uniform to fail
exit /b 1

:ok
type "%LOG_FILE%"
if exist "%OUT_FILE%" exit /b 1
exit /b 0
