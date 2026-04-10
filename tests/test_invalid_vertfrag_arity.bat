@echo off
setlocal

set SCRIPT_DIR=%~dp0
set RAWGL_BIN=%SCRIPT_DIR%..\bin\RawGL\RawGL.exe
if not "%RAWGL_BIN_OVERRIDE%"=="" set RAWGL_BIN=%RAWGL_BIN_OVERRIDE%
set LOG_FILE=%SCRIPT_DIR%outputs\invalid_vertfrag_arity.log
set OUT_FILE=%SCRIPT_DIR%outputs\invalid_vertfrag_arity.exr

if exist "%LOG_FILE%" del /f /q "%LOG_FILE%"
if exist "%OUT_FILE%" del /f /q "%OUT_FILE%"

"%RAWGL_BIN%" ^
  --verbosity 5 ^
  --pass_vertfrag "%SCRIPT_DIR%shaders\empty.vert" "%SCRIPT_DIR%shaders\pass1.frag" "%SCRIPT_DIR%shaders\pass2.frag" ^
  --pass_size 8 8 ^
  --out OutSample "%OUT_FILE%" >"%LOG_FILE%" 2>&1

if not errorlevel 1 exit /b 1
if exist "%OUT_FILE%" exit /b 1
findstr /c:"pass_vertfrag: must have one combined shader file or two stage files" "%LOG_FILE%" >nul || exit /b 1

endlocal
