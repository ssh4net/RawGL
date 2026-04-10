@echo off
setlocal

set SCRIPT_DIR=%~dp0
set RAWGL_BIN=%SCRIPT_DIR%..\bin\RawGL\RawGL.exe
if not "%RAWGL_BIN_OVERRIDE%"=="" set RAWGL_BIN=%RAWGL_BIN_OVERRIDE%
set OUT_FILE=%SCRIPT_DIR%outputs\output_array_reflection.exr

if exist "%OUT_FILE%" del /f /q "%OUT_FILE%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%SCRIPT_DIR%shaders\empty.vert" "%SCRIPT_DIR%shaders\output_array.frag" ^
  --pass_size 8 8 ^
  --out OutSample "%OUT_FILE%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32

if errorlevel 1 exit /b 1
if not exist "%OUT_FILE%" exit /b 1

endlocal
