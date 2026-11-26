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
#include <vector>

namespace vakit {

struct ContextMenuCallbacks {
    std::function<void()> onToggleFlyout;
    std::function<void()> onStopSound;
    std::function<void()> onOpenImsakiye;
    std::function<void()> onChangeLocation;
    std::function<void()> onRefresh;
    std::function<void(bool)> onToggleAllNotifications;
    std::function<void()> onMute1h;
    std::function<void()> onMuteToday;
    std::function<void()> onUnmute;
    std::function<void()> onTestNotification;
    std::function<void()> onOpenSettings;
    std::function<void()> onOpenPro;
    std::function<void()> onExit;
};

class ContextMenuPopup {
public:
    ContextMenuPopup();
    ~ContextMenuPopup();

    void Show(
        int anchorScreenX,
        int anchorScreenY,
        const RECT& bandRect,
        bool bandVisible,
        const AppSettings& settings,
        const PrayerCountdownState* latestState,
        bool isPaidPro,
        ContextMenuCallbacks callbacks);

    bool IsOpen() const { return hwnd_ != nullptr && IsWindowVisible(hwnd_); }
    void CloseNow();
    ULONGLONG LastClosedTick() const { return lastClosedTick_; }
    void EnsureOnTop();

private:
    enum class MenuAction {
        None = 0,
        HeaderFlyout,
        StopSound,
        Imsakiye,
        Location,
        Refresh,
        NotifToggleAccordion,
        NotifAllSwitch,
        NotifUnmute,
        NotifMute1h,
        NotifMuteToday,
        NotifTest,
        Settings,
        Pro,
        Exit
    };

    struct ItemHit {
        MenuAction action = MenuAction::None;
        RECT rc{};
        bool inSubCard = false;
    };

    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void RebuildLayout(int& outWidth, int& outHeight);
    void PositionWindow(int winW, int winH);
    void PaintClient(HDC hdc, int width, int height);
    int HitTestIndex(int x, int y) const;
    void TriggerAction(MenuAction action);

    HWND hwnd_ = nullptr;
    double scale_ = 1.0;
    int anchorScreenX_ = 0;
    int anchorScreenY_ = 0;
    RECT bandRect_{};
    bool bandVisible_ = false;
    bool isPaidPro_ = false;
    bool hasState_ = false;
    PrayerCountdownState state_{};
    AppSettings settings_{};
    ContextMenuCallbacks callbacks_{};

    bool notifExpanded_ = false;
    bool trackingLeave_ = false;
    bool closing_ = false;
    ULONGLONG lastClosedTick_ = 0;
    int hoverIndex_ = -1;
    int focusIndex_ = -1;
    std::vector<ItemHit> items_;
    RECT notifSubCardRc_{};
    float divider1Y_ = 0.0f;
    float divider2Y_ = 0.0f;
    int mutedUntilMin_ = 0;
    bool isMuted_ = false;
};

} // namespace vakit
