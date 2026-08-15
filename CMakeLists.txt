cmake_minimum_required(VERSION 3.16)
project(WindowLayout LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Version is injected only via generated AppVersion.h (quoted string).
# Never use target_compile_definitions for APP_VERSION (MSVC numeric parse).
# Never put a default APP_VERSION in src/Version.h (same-dir include wins).
if(NOT DEFINED APP_VERSION OR APP_VERSION STREQUAL "")
    set(APP_VERSION "0.0.0-dev")
endif()

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")
configure_file(
    "${CMAKE_SOURCE_DIR}/src/AppVersion.h.in"
    "${GENERATED_DIR}/AppVersion.h"
    @ONLY
)

add_executable(WindowLayout WIN32
    src/main.cpp
    src/App.cpp
    src/WindowManager.cpp
    src/MonitorManager.cpp
    src/UpdateManager.cpp
    src/app.rc
)

# generated/ must be first so #include "AppVersion.h" resolves correctly.
# src/Version.h includes AppVersion.h; AppVersion.h exists only under generated/.
target_include_directories(WindowLayout PRIVATE
    "${GENERATED_DIR}"
    src
)

target_compile_definitions(WindowLayout PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(WindowLayout PRIVATE /W4 /utf-8)
else()
    target_compile_options(WindowLayout PRIVATE -Wall -Wextra)
endif()

target_link_libraries(WindowLayout PRIVATE
    user32
    gdi32
    shell32
    comctl32
    ole32
    dwmapi
    shcore
    psapi
    winhttp
)

message(STATUS "WindowLayout APP_VERSION=${APP_VERSION}")
