cmake_minimum_required(VERSION 3.16)
project(WindowLayout LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Version must be injected as a proper C string literal.
# Do NOT pass it via target_compile_definitions: MSVC treats
# /DAPP_VERSION=0.1.6 as a numeric macro (shows up as "0" in UI).
if(NOT DEFINED APP_VERSION OR APP_VERSION STREQUAL "")
    set(APP_VERSION "0.1.6")
endif()

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")
configure_file(
    "${CMAKE_SOURCE_DIR}/src/Version.h.in"
    "${GENERATED_DIR}/Version.h"
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

# Generated Version.h must win over src/Version.h
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
