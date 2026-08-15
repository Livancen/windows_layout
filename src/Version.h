#pragma once

// Default for IDE / local includes. CMake and build.bat prefer a generated
// header from the build directory (which always has a quoted string version).
#ifndef APP_VERSION
#define APP_VERSION "0.1.7"
#endif

#ifndef APP_REPO_OWNER
#define APP_REPO_OWNER "Livancen"
#endif

#ifndef APP_REPO_NAME
#define APP_REPO_NAME "windows_layout"
#endif

#ifndef APP_RELEASE_ASSET
#define APP_RELEASE_ASSET "WindowLayout-windows-x64.zip"
#endif

#define APP_REPO_OWNER_W L"Livancen"
#define APP_REPO_NAME_W L"windows_layout"
