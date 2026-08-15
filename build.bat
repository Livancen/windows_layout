@echo off
setlocal
cd /d "%~dp0"

set CLANG="C:\Program Files\LLVM\bin\clang++.exe"
set LLVMRC="C:\Program Files\LLVM\bin\llvm-rc.exe"
set OUT=build\WindowLayout.exe
set RES=build\app.res
set GEN=build\generated
set SRC=src\main.cpp src\App.cpp src\WindowManager.cpp src\MonitorManager.cpp src\UpdateManager.cpp
if not defined APP_VERSION set APP_VERSION=0.1.8

if not exist build mkdir build
if not exist %GEN% mkdir %GEN%

echo Writing %GEN%\Version.h (APP_VERSION=%APP_VERSION%)...
(
  echo #pragma once
  echo #ifndef APP_VERSION
  echo #define APP_VERSION "%APP_VERSION%"
  echo #endif
  echo #ifndef APP_REPO_OWNER
  echo #define APP_REPO_OWNER "Livancen"
  echo #endif
  echo #ifndef APP_REPO_NAME
  echo #define APP_REPO_NAME "windows_layout"
  echo #endif
  echo #ifndef APP_RELEASE_ASSET
  echo #define APP_RELEASE_ASSET "WindowLayout-windows-x64.zip"
  echo #endif
  echo #define APP_REPO_OWNER_W L"Livancen"
  echo #define APP_REPO_NAME_W L"windows_layout"
) > %GEN%\Version.h

echo Compiling resources...
%LLVMRC% /nologo /fo %RES% src\app.rc /I src
if errorlevel 1 (
  echo Resource compile failed.
  exit /b 1
)

echo Building WindowLayout v%APP_VERSION%...
%CLANG% %SRC% %RES% -o %OUT% ^
  -std=c++17 -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
  -I%GEN% -Isrc ^
  -luser32 -lgdi32 -lshell32 -lcomctl32 -lole32 -ldwmapi -lshcore -lpsapi -lwinhttp ^
  -fuse-ld=lld -Wl,/subsystem:windows -Wl,/ENTRY:wWinMainCRTStartup

if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Build OK: %OUT%
endlocal
