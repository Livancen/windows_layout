@echo off
setlocal
cd /d "%~dp0"

set CLANG="C:\Program Files\LLVM\bin\clang++.exe"
set OUT=build\WindowLayout.exe
set SRC=src\main.cpp src\App.cpp src\WindowManager.cpp src\MonitorManager.cpp src\UpdateManager.cpp
if not defined APP_VERSION set APP_VERSION=0.1.0

if not exist build mkdir build

echo Building WindowLayout v%APP_VERSION%...
%CLANG% %SRC% -o %OUT% ^
  -std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
  -DAPP_VERSION=\"%APP_VERSION%\" ^
  -Isrc ^
  -luser32 -lgdi32 -lshell32 -lcomctl32 -lole32 -ldwmapi -lshcore -lpsapi -lwinhttp ^
  -fuse-ld=lld -Wl,/subsystem:windows -Wl,/ENTRY:wWinMainCRTStartup

if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Build OK: %OUT%
endlocal
