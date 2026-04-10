@echo off
setlocal

set RAWGL_BIN=%~dp0..\bin\RawGL\RawGL.exe
if not "%1"=="" set RAWGL_BIN=%~1

set OUT1=%~dp0outputs\atomic_too_many_values.exr
set LOG1=%~dp0outputs\atomic_too_many_values.log
set OUT2=%~dp0outputs\atomic_unknown_mode.exr
set LOG2=%~dp0outputs\atomic_unknown_mode.log

if exist "%OUT1%" del /f /q "%OUT1%"
if exist "%LOG1%" del /f /q "%LOG1%"
if exist "%OUT2%" del /f /q "%OUT2%"
if exist "%LOG2%" del /f /q "%LOG2%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_comp "%~dp0shaders\atomic_counter.comp" ^
  --pass_size 1 1 ^
  --pass_workgroupsize 1 1 ^
  --atomic cntr counter0 5 6 ^
  --out o_out0 "%OUT1%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32 >"%LOG1%" 2>&1
if not errorlevel 1 exit /b 1
if exist "%OUT1%" exit /b 1

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_comp "%~dp0shaders\atomic_counter.comp" ^
  --pass_size 1 1 ^
  --pass_workgroupsize 1 1 ^
  --atomic nope counter0 5 ^
  --out o_out0 "%OUT2%" ^
  --out_format rgba32f ^
  --out_channels 4 ^
  --out_alpha_channel 3 ^
  --out_bits 32 >"%LOG2%" 2>&1
if not errorlevel 1 exit /b 1
if exist "%OUT2%" exit /b 1

type "%LOG1%"
type "%LOG2%"
exit /b 0
