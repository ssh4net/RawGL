@echo off
setlocal

set SCRIPT_DIR=%~dp0
set RAWGL_BIN=%RAWGL_BIN%
if "%RAWGL_BIN%"=="" set RAWGL_BIN=%SCRIPT_DIR%..\build_vs2022\Release\RawGL.exe
set OIIOTOOL_BIN=%RAWGL_OIIOTOOL%
if "%OIIOTOOL_BIN%"=="" set OIIOTOOL_BIN=oiiotool.exe
set OUT_FILE=%SCRIPT_DIR%outputs\mesh_obj_smoke.exr

del "%OUT_FILE%" >nul 2>nul

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%SCRIPT_DIR%shaders\empty.vert" "%SCRIPT_DIR%shaders\solid_white.frag" ^
  --pass_size 8 8 ^
  --pass_mesh mesh tris true rend tr "%SCRIPT_DIR%inputs\fullscreen_triangle.obj" ^
  --out OutSample "%OUT_FILE%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32
if errorlevel 1 exit /b 1

if not exist "%OUT_FILE%" exit /b 1

"%OIIOTOOL_BIN%" "%OUT_FILE%" --printstats | findstr /C:"Stats Avg: 1.000000 1.000000 1.000000 1.000000" >nul
if errorlevel 1 exit /b 1

exit /b 0
