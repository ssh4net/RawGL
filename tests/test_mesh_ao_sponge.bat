@echo off
setlocal

set SCRIPT_DIR=%~dp0
set RAWGL_BIN=%SCRIPT_DIR%..\bin\RawGL\RawGL.exe
if not "%RAWGL_BIN_OVERRIDE%"=="" set RAWGL_BIN=%RAWGL_BIN_OVERRIDE%
set OUT_FILE=%SCRIPT_DIR%outputs\mesh_ao_sponge.exr

if exist "%OUT_FILE%" del /f /q "%OUT_FILE%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%SCRIPT_DIR%shaders\mesh_ao.vert" "%SCRIPT_DIR%shaders\mesh_ao.frag" ^
  --pass_size 256 256 ^
  --bg_color 0 0 0 1 ^
  --pass_mesh mesh tris true rend tr "%SCRIPT_DIR%inputs\sponge.ply" ^
  --out OutSample "%OUT_FILE%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32

if errorlevel 1 exit /b 1
if not exist "%OUT_FILE%" exit /b 1

endlocal
