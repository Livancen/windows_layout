@echo off
setlocal
cd /d "%~dp0"

set CLANG="C:\Program Files\LLVM\bin\clang++.exe"
set OUT=build\WindowLayout.exe
set SRC=src\main.cpp src\App.cpp src\WindowManager.cpp src\MonitorManager.cpp

if not exist build mkdir build

echo Building WindowLayout...
%CLANG% %SRC% -o %OUT% ^
  -std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
  -Isrc ^
  -luser32 -lgdi32 -lshell32 -lcomctl32 -lole32 -ldwmapi -lshcore -lpsapi ^
  -fuse-ld=lld -Wl,/subsystem:windows -Wl,/ENTRY:wWinMainCRTStartup

if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Build OK: %OUT%
endlocal
