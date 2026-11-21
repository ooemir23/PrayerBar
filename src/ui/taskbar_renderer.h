#pragma once

#include "../core/formatter.h"
#include "../core/models.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace vakit {

struct PillRenderResult {
    int status = -1;
    int nonZeroPixels = 0;
};

int MeasureDesiredPillWidth(const FormattedTaskbarDisplay& display, const AppSettings& settings, double scale);

void RenderPillToLayeredWindow(
    HWND hwnd,
    int x,
    int y,
    int width,
    int height,
    double scale,
    const FormattedTaskbarDisplay& display,
    const AppSettings& settings,
    bool isHovered,
    bool isFlyoutOpen,
    bool isTaskbarLight,
    bool useOpaqueBackdrop);

PillRenderResult SmokeTestPillRender(
    const FormattedTaskbarDisplay& display,
    const AppSettings& settings,
    double scale,
    bool useOpaqueBackdrop);

HICON CreateTrayIconHandle(
    const PrayerCountdownState& state,
    const AppSettings& settings,
    int iconSize,
    bool isTaskbarLight);

} // namespace vakit
