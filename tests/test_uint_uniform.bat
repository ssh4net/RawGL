@echo off
setlocal

set RAWGL_BIN=%~dp0..\bin\RawGL\RawGL.exe
if not "%1"=="" set RAWGL_BIN=%~1

set OUT_FILE=%~dp0outputs\uint_uniform.exr
if exist "%OUT_FILE%" del /f /q "%OUT_FILE%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_comp "%~dp0shaders\uint_uniform.comp" ^
  --pass_size 1 1 ^
  --pass_workgroupsize 1 1 ^
  --in u_value 4000000000 ^
  --out o_out0 "%OUT_FILE%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32

if errorlevel 1 exit /b 1
if not exist "%OUT_FILE%" exit /b 1
