#pragma once

#include "../core/models.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace vakit {

struct TaskbarPlacement {
    bool shouldShow = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    double scale = 1.0;
    RECT taskbarRect{};
    bool overlapsTrayArea = false;
};

double GetTaskbarScale();
TaskbarPlacement ComputeTaskbarPlacement(int desiredPillWidth, const AppSettings& settings, HWND ownBandHwnd);

} // namespace vakit
