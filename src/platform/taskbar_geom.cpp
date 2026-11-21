#include "taskbar_geom.h"
#include "theme_utils.h"

#include <shellapi.h>

#include <algorithm>
#include <cmath>

namespace vakit {

namespace {

bool IsTaskbarSlidOut(HWND taskbarHwnd, const RECT& taskbarRect, double scale) {
    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);
    abd.hWnd = taskbarHwnd;
    UINT_PTR state = SHAppBarMessage(ABM_GETSTATE, &abd);
    if ((state & ABS_AUTOHIDE) == 0) return false;

    HMONITOR mon = MonitorFromWindow(taskbarHwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) return false;

    int threshold = static_cast<int>(std::llround(10.0 * scale));
    return taskbarRect.top >= mi.rcMonitor.bottom - threshold ||
           taskbarRect.bottom <= mi.rcMonitor.top + threshold ||
           taskbarRect.left >= mi.rcMonitor.right - threshold ||
           taskbarRect.right <= mi.rcMonitor.left + threshold;
}

bool IsFullscreenActiveOnTaskbarMonitor(HWND taskbarHwnd, HWND ownBandHwnd) {
    QUERY_USER_NOTIFICATION_STATE quns{};
    if (SUCCEEDED(SHQueryUserNotificationState(&quns))) {
        if (quns == QUNS_BUSY || quns == QUNS_RUNNING_D3D_FULL_SCREEN || quns == QUNS_PRESENTATION_MODE) {
            return true;
        }
    }

    HWND fg = GetForegroundWindow();
    if (!fg || fg == taskbarHwnd || fg == ownBandHwnd) return false;

    wchar_t cls[128]{};
    int len = GetClassNameW(fg, cls, 128);
    if (len > 0) {
        if (wcscmp(cls, L"Shell_TrayWnd") == 0 ||
            wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0 ||
            wcscmp(cls, L"Progman") == 0 ||
            wcscmp(cls, L"WorkerW") == 0 ||
            wcscmp(cls, L"NotifyIconOverflowWindow") == 0 ||
            wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0) {
            return false;
        }
    }

    HMONITOR tbMon = MonitorFromWindow(taskbarHwnd, MONITOR_DEFAULTTONEAREST);
    HMONITOR fgMon = MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST);
    if (tbMon != fgMon) return false;

    RECT fgRect{};
    if (!GetWindowRect(fg, &fgRect)) return false;

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(tbMon, &mi)) return false;

    constexpr int kTol = 2;
    return fgRect.left <= mi.rcMonitor.left + kTol &&
           fgRect.top <= mi.rcMonitor.top + kTol &&
           fgRect.right >= mi.rcMonitor.right - kTol &&
           fgRect.bottom >= mi.rcMonitor.bottom - kTol;
}

} // namespace

double GetTaskbarScale() {
    HWND taskbarHwnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!taskbarHwnd) return 1.0;
    UINT dpi = GetDpiForWindowCompat(taskbarHwnd);
    return dpi >= 96 ? static_cast<double>(dpi) / 96.0 : 1.0;
}

TaskbarPlacement ComputeTaskbarPlacement(int desiredPillWidth, const AppSettings& settings, HWND ownBandHwnd) {
    TaskbarPlacement out;
    out.width = desiredPillWidth;

    HWND taskbarHwnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!taskbarHwnd || !IsWindowVisible(taskbarHwnd)) {
        return out;
    }

    RECT taskbarRect{};
    if (!GetWindowRect(taskbarHwnd, &taskbarRect)) return out;
    int tbW = taskbarRect.right - taskbarRect.left;
    int tbH = taskbarRect.bottom - taskbarRect.top;
    if (tbW <= 0 || tbH <= 0) return out;

    UINT dpi = GetDpiForWindowCompat(taskbarHwnd);
    double scale = dpi >= 96 ? static_cast<double>(dpi) / 96.0 : 1.0;
    out.scale = scale;
    out.taskbarRect = taskbarRect;

    if (IsTaskbarSlidOut(taskbarHwnd, taskbarRect, scale) ||
        IsFullscreenActiveOnTaskbarMonitor(taskbarHwnd, ownBandHwnd)) {
        return out;
    }

    int pillHeight = std::clamp(tbH - static_cast<int>(std::llround(8.0 * scale)),
                                static_cast<int>(std::llround(28.0 * scale)), tbH);
    int y = taskbarRect.top + (tbH - pillHeight) / 2;

    int offsetScaled = static_cast<int>(std::llround(settings.horizontalOffsetPx * scale));
    HWND trayNotify = FindWindowExW(taskbarHwnd, nullptr, L"TrayNotifyWnd", nullptr);
    RECT trayRect{};
    bool hasTrayRect = trayNotify != nullptr &&
                       GetWindowRect(trayNotify, &trayRect) &&
                       (trayRect.right - trayRect.left) > 0 &&
                       trayRect.left > taskbarRect.left;

    int x = 0;
    switch (settings.dockPosition) {
        case TaskbarDockPosition::LeftSide:
            x = taskbarRect.left + static_cast<int>(std::llround(8.0 * scale)) + offsetScaled;
            break;

        case TaskbarDockPosition::Center:
            x = taskbarRect.left + (tbW - desiredPillWidth) / 2 + offsetScaled;
            break;

        case TaskbarDockPosition::FarRight:
            x = taskbarRect.right - desiredPillWidth - static_cast<int>(std::llround(2.0 * scale)) + offsetScaled;
            break;

        case TaskbarDockPosition::RightNearTray:
        default:
            if (hasTrayRect) {
                int rightAnchor = trayRect.left - static_cast<int>(std::llround(6.0 * scale)) + offsetScaled;
                x = rightAnchor - desiredPillWidth;
            } else {
                int fallbackRight = taskbarRect.right - static_cast<int>(std::llround(235.0 * scale)) + offsetScaled;
                x = fallbackRight - desiredPillWidth;
            }
            break;
    }

    int minX = taskbarRect.left + static_cast<int>(std::llround(2.0 * scale));
    int maxX = std::max(minX, static_cast<int>(taskbarRect.right) - desiredPillWidth - static_cast<int>(std::llround(2.0 * scale)));
    x = std::clamp(x, minX, maxX);

    bool overlapsTray = (settings.dockPosition == TaskbarDockPosition::FarRight) ||
                        (hasTrayRect && (x + desiredPillWidth) > trayRect.left && x < trayRect.right);

    out.shouldShow = true;
    out.x = x;
    out.y = y;
    out.width = desiredPillWidth;
    out.height = pillHeight;
    out.overlapsTrayArea = overlapsTray;
    return out;
}

} // namespace vakit
