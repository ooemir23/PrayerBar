#pragma once

#include "../core/models.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <functional>
#include <string>

namespace vakit {

class TaskbarBandWindow {
public:
    TaskbarBandWindow();
    ~TaskbarBandWindow();

    HWND Handle() const { return hwnd_; }
    RECT CurrentRect() const { return currentRect_; }

    void SetCallbacks(
        std::function<void(RECT)> onLeftClick,
        std::function<void()> onRefresh,
        std::function<void()> onOpenSettings,
        std::function<void(TaskbarDisplayStyle)> onQuickStyle,
        std::function<void()> onExit,
        std::function<void()> onVisualInvalidated);

    void SetExtraMenuCallbacks(
        std::function<void()> onOpenImsakiye,
        std::function<void(FlyoutDesignStyle)> onQuickFlyoutDesign,
        std::function<void(bool)> onToggleTrayIcon,
        std::function<void(bool)> onToggleTaskbarPill,
        std::function<void()> onOpenPro = nullptr,
        std::function<bool()> isPaidProFn = nullptr);

    void SetFlyoutOpenState(bool isOpen, const PrayerCountdownState* latestState, const AppSettings& settings);
    void Update(const PrayerCountdownState& state, const AppSettings& settings, bool forceRedraw = false);
    void ShowContextMenu(int screenX, int screenY);

private:
    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static void CALLBACK WinEventStatic(HWINEVENTHOOK hook, DWORD eventType, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    HWND cachedTaskbarHwnd_ = nullptr;
    HWINEVENTHOOK hookForeground_ = nullptr;
    HWINEVENTHOOK hookLocation_ = nullptr;
    UINT taskbarCreatedMsg_ = 0;

    bool isHovered_ = false;
    bool isTrackingLeave_ = false;
    bool isFlyoutOpen_ = false;
    std::wstring lastRenderSignature_;
    RECT currentRect_{};
    AppSettings currentSettings_{};

    std::function<void(RECT)> onLeftClick_;
    std::function<void()> onRefresh_;
    std::function<void()> onOpenSettings_;
    std::function<void(TaskbarDisplayStyle)> onQuickStyle_;
    std::function<void()> onExit_;
    std::function<void()> onVisualInvalidated_;

    std::function<void()> onOpenImsakiye_;
    std::function<void(FlyoutDesignStyle)> onQuickFlyoutDesign_;
    std::function<void(bool)> onToggleTrayIcon_;
    std::function<void(bool)> onToggleTaskbarPill_;
    std::function<void()> onOpenPro_;
    std::function<bool()> isPaidProFn_;
};

} // namespace vakit
