cmake_minimum_required(VERSION 3.16)
project(WindowLayout LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED APP_VERSION)
    set(APP_VERSION "0.1.4")
endif()

add_executable(WindowLayout WIN32
    src/main.cpp
    src/App.cpp
    src/WindowManager.cpp
    src/MonitorManager.cpp
    src/UpdateManager.cpp
    src/app.rc
)

target_include_directories(WindowLayout PRIVATE src)

target_compile_definitions(WindowLayout PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    APP_VERSION="${APP_VERSION}"
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
