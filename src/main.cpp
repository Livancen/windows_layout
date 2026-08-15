#include "App.h"
#include "UpdateManager.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    // Updater mode: replace old exe then relaunch (no UI).
    if (UpdateManager::TryHandleSelfUpdate(lpCmdLine)) {
        return 0;
    }

    // DPI awareness for multi-monitor coordinate accuracy
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    return App::Instance().Run(hInstance, nCmdShow);
}
