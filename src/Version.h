#pragma once

// APP_VERSION comes only from generated AppVersion.h (build/generated).
// Do NOT define APP_VERSION here — files under src/ would shadow a generated Version.h.
#include "AppVersion.h"

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
