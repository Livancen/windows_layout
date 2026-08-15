#pragma once

// Keep as a plain string so CI can override with -DAPP_VERSION=...
#ifndef APP_VERSION
#define APP_VERSION "0.1.5"
#endif

// Wide form for UI (string concatenation, no printf formatting).
#define APP_VERSION_W L"" APP_VERSION

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
