#include "App.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    // DPI awareness for multi-monitor coordinate accuracy
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    return App::Instance().Run(hInstance, nCmdShow);
}
