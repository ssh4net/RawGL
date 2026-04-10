@echo off
setlocal

set RAWGL_BIN=%~dp0..\bin\RawGL\RawGL.exe
if not "%1"=="" set RAWGL_BIN=%~1

set OUT_FILE=%~dp0outputs\mesh_ply_smoke.exr
if exist "%OUT_FILE%" del /f /q "%OUT_FILE%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%~dp0shaders\empty.vert" "%~dp0shaders\solid_white.frag" ^
  --pass_size 8 8 ^
  --pass_mesh mesh tris true rend tr "%~dp0inputs\fullscreen_triangle.ply" ^
  --out OutSample "%OUT_FILE%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32

if errorlevel 1 exit /b 1
if not exist "%OUT_FILE%" exit /b 1
