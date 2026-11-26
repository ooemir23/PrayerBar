#include "taskbar_band.h"
#include "context_menu.h"
#include "taskbar_renderer.h"
#include "../core/commercial_config.h"
#include "../core/formatter.h"
#include "../core/i18n.h"
#include "../platform/audio_player.h"
#include "../platform/taskbar_geom.h"
#include "../platform/theme_utils.h"
#include "../core/schedule_service.h"

#include <shellapi.h>

#include <cmath>
#include <cstdio>

namespace vakit {

namespace {

constexpr wchar_t kBandClassName[] = L"PrayerBarTaskbarBand";

TaskbarBandWindow* g_bandInstance = nullptr;

} // namespace

TaskbarBandWindow::TaskbarBandWindow() {
    g_bandInstance = this;
    taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.lpszClassName = kBandClassName;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TOPMOST,
        kBandClassName,
        L"PrayerBar",
        WS_POPUP,
        0, 0, 110, 40,
        nullptr, nullptr, hInst, this);

    cachedTaskbarHwnd_ = FindWindowW(L"Shell_TrayWnd", nullptr);
    contextMenu_ = std::make_unique<ContextMenuPopup>();

    hookForeground_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        WinEventStatic,
        0, 0,
        WINEVENT_OUTOFCONTEXT);

    RehookTaskbarLocationEvent();
}

void TaskbarBandWindow::RehookTaskbarLocationEvent() {
    if (hookLocation_) {
        UnhookWinEvent(hookLocation_);
        hookLocation_ = nullptr;
    }
    cachedTaskbarHwnd_ = FindWindowW(L"Shell_TrayWnd", nullptr);
    DWORD tbPid = 0;
    DWORD tbTid = cachedTaskbarHwnd_ ? GetWindowThreadProcessId(cachedTaskbarHwnd_, &tbPid) : 0;
    if (tbPid != 0 && tbTid != 0) {
        hookLocation_ = SetWinEventHook(
            EVENT_OBJECT_LOCATIONCHANGE,
            EVENT_OBJECT_LOCATIONCHANGE,
            nullptr,
            WinEventStatic,
            tbPid, tbTid,
            WINEVENT_OUTOFCONTEXT);
    }
}

TaskbarBandWindow::~TaskbarBandWindow() {
    contextMenu_.reset();
    if (hookForeground_) {
        UnhookWinEvent(hookForeground_);
        hookForeground_ = nullptr;
    }
    if (hookLocation_) {
        UnhookWinEvent(hookLocation_);
        hookLocation_ = nullptr;
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (g_bandInstance == this) {
        g_bandInstance = nullptr;
    }
}

void TaskbarBandWindow::SetCallbacks(
    std::function<void(RECT)> onLeftClick,
    std::function<void()> onRefresh,
    std::function<void()> onOpenSettings,
    std::function<void(TaskbarDisplayStyle)> onQuickStyle,
    std::function<void()> onExit,
    std::function<void()> onVisualInvalidated) {

    onLeftClick_ = std::move(onLeftClick);
    onRefresh_ = std::move(onRefresh);
    onOpenSettings_ = std::move(onOpenSettings);
    onQuickStyle_ = std::move(onQuickStyle);
    onExit_ = std::move(onExit);
    onVisualInvalidated_ = std::move(onVisualInvalidated);
}

void TaskbarBandWindow::SetExtraMenuCallbacks(
    std::function<void()> onOpenImsakiye,
    std::function<void(FlyoutDesignStyle)> onQuickFlyoutDesign,
    std::function<void(bool)> onToggleTrayIcon,
    std::function<void(bool)> onToggleTaskbarPill,
    std::function<void()> onOpenPro,
    std::function<bool()> isPaidProFn) {

    onOpenImsakiye_ = std::move(onOpenImsakiye);
    onQuickFlyoutDesign_ = std::move(onQuickFlyoutDesign);
    onToggleTrayIcon_ = std::move(onToggleTrayIcon);
    onToggleTaskbarPill_ = std::move(onToggleTaskbarPill);
    onOpenPro_ = std::move(onOpenPro);
    isPaidProFn_ = std::move(isPaidProFn);
}

void TaskbarBandWindow::SetFlyoutOpenState(bool isOpen, const PrayerCountdownState* latestState, const AppSettings& settings) {
    if (isFlyoutOpen_ == isOpen) return;
    isFlyoutOpen_ = isOpen;
    lastRenderSignature_.clear();
    if (latestState) {
        Update(*latestState, settings, true);
    }
}

void TaskbarBandWindow::Update(const PrayerCountdownState& state, const AppSettings& settings, bool forceRedraw, bool raiseZOrder) {
    if (!hwnd_) return;
    currentSettings_ = settings;
    lastState_ = state;
    hasState_ = true;

    if (!settings.showTaskbarPill) {
        if (IsWindowVisible(hwnd_)) {
            ShowWindow(hwnd_, SW_HIDE);
        }
        return;
    }

    double scale = GetTaskbarScale();
    FormattedTaskbarDisplay display = FormatTaskbar(state, settings);
    wchar_t measBuf[256];
    swprintf_s(measBuf, L"%ls|%ls|%d|%d|%d|%d|%d|%lld",
               display.line1.c_str(), display.line2.c_str(),
               display.hasLine2 ? 1 : 0, display.showPhaseIcon ? 1 : 0,
               display.showDot ? 1 : 0, settings.showTaskbarLogo ? 1 : 0,
               static_cast<int>(settings.fontSize),
               static_cast<long long>(std::llround(scale * 100.0)));
    std::wstring measSig(measBuf);
    if (forceRedraw || cachedMeasuredWidth_ <= 0 || measSig != lastMeasureSig_) {
        lastMeasureSig_ = std::move(measSig);
        cachedMeasuredWidth_ = MeasureDesiredPillWidth(display, settings, scale);
    }
    int desiredWidth = cachedMeasuredWidth_;
    TaskbarPlacement placement = ComputeTaskbarPlacement(desiredWidth, settings, hwnd_);

    if (!placement.shouldShow) {
        if (IsWindowVisible(hwnd_)) {
            ShowWindow(hwnd_, SW_HIDE);
        }
        return;
    }

    bool isLight = IsTaskbarLight(settings.themeMode);
    bool useOpaque = settings.opaqueTaskbarBackground || placement.overlapsTrayArea;
    int progressBucket = settings.showProgressBar ? static_cast<int>(std::llround(display.progress * 100.0)) : -1;

    wchar_t sigBuf[512];
    swprintf_s(sigBuf, L"%d|%d|%d|%d|%ls|%ls|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d",
               placement.x, placement.y, placement.width, placement.height,
               display.line1.c_str(), display.line2.c_str(),
               display.showPhaseIcon ? 1 : 0, display.showDot ? 1 : 0,
               static_cast<int>(display.nextPrayer), display.isUrgent ? 1 : 0,
               progressBucket, isHovered_ ? 1 : 0, isFlyoutOpen_ ? 1 : 0,
               isLight ? 1 : 0, useOpaque ? 1 : 0, static_cast<int>(settings.fontSize),
               settings.useSystemAccentColor ? 1 : 0, static_cast<int>(settings.language),
               settings.showTaskbarFrame ? 1 : 0, settings.showTaskbarLogo ? 1 : 0);
    std::wstring sig(sigBuf);

    bool wasVisible = (IsWindowVisible(hwnd_) != FALSE);
    RECT prevRect = currentRect_;
    currentRect_ = {placement.x, placement.y, placement.x + placement.width, placement.y + placement.height};

    if (forceRedraw || sig != lastRenderSignature_) {
        lastRenderSignature_ = std::move(sig);
        RenderPillToLayeredWindow(
            hwnd_,
            placement.x,
            placement.y,
            placement.width,
            placement.height,
            placement.scale,
            display,
            settings,
            isHovered_,
            isFlyoutOpen_,
            isLight,
            useOpaque);
    }

    if (forceRedraw || raiseZOrder || !wasVisible || !EqualRect(&prevRect, &currentRect_)) {
        SetWindowPos(
            hwnd_,
            HWND_TOPMOST,
            placement.x,
            placement.y,
            placement.width,
            placement.height,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    if (contextMenu_ && contextMenu_->IsOpen()) {
        contextMenu_->EnsureOnTop();
    }
}

void TaskbarBandWindow::SetMenuActionCallbacks(
    std::function<void(const std::function<void(AppSettings&)>&)> onMutateSettings,
    std::function<void(int)> onOpenSettingsTab,
    std::function<void()> onTestNotification) {
    onMutateSettings_ = std::move(onMutateSettings);
    onOpenSettingsTab_ = std::move(onOpenSettingsTab);
    onTestNotification_ = std::move(onTestNotification);
}

void TaskbarBandWindow::ShowContextMenu(int screenX, int screenY) {
    if (!contextMenu_) {
        contextMenu_ = std::make_unique<ContextMenuPopup>();
    }
    if (contextMenu_->IsOpen()) {
        contextMenu_->CloseNow();
        return;
    }
    if ((GetTickCount64() - contextMenu_->LastClosedTick()) < 200) {
        return;
    }

    auto mutate = [this](const std::function<void(AppSettings&)>& f) {
        if (onMutateSettings_) onMutateSettings_(f);
    };

    ContextMenuCallbacks cb;
    cb.onToggleFlyout = [this]() {
        if (onLeftClick_) onLeftClick_(currentRect_);
    };
    cb.onStopSound = []() {
        AudioPlayer::Stop();
    };
    cb.onOpenImsakiye = [this]() {
        if (onOpenImsakiye_) onOpenImsakiye_();
    };
    cb.onChangeLocation = [this]() {
        if (onOpenSettingsTab_) onOpenSettingsTab_(1);
    };
    cb.onRefresh = [this]() {
        if (onRefresh_) onRefresh_();
    };
    cb.onToggleAllNotifications = [mutate](bool enabled) {
        mutate([enabled](AppSettings& a) { a.SetAllNotificationsEnabled(enabled); });
    };
    cb.onMute1h = [mutate]() {
        LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
        int nowMin = static_cast<int>(now.ToEpochSeconds() / 60);
        mutate([nowMin](AppSettings& a) { a.notifyMuteUntilMin = nowMin + 60; });
    };
    cb.onMuteToday = [mutate]() {
        LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
        int nowMin = static_cast<int>(now.ToEpochSeconds() / 60);
        int minsLeftToday = 24 * 60 - (now.hour * 60 + now.minute);
        mutate([nowMin, minsLeftToday](AppSettings& a) { a.notifyMuteUntilMin = nowMin + minsLeftToday; });
    };
    cb.onUnmute = [mutate]() {
        mutate([](AppSettings& a) { a.notifyMuteUntilMin = 0; });
    };
    cb.onTestNotification = [this]() {
        if (onTestNotification_) onTestNotification_();
    };
    cb.onOpenSettings = [this]() {
        if (onOpenSettings_) onOpenSettings_();
    };
    cb.onOpenPro = [this]() {
        if (onOpenPro_) onOpenPro_();
    };
    cb.onExit = [this]() {
        if (onExit_) onExit_();
    };

    const bool bandVisible = hwnd_ && IsWindowVisible(hwnd_);
    const bool isPaidPro = isPaidProFn_ ? isPaidProFn_() : false;
    contextMenu_->Show(
        screenX,
        screenY,
        currentRect_,
        bandVisible,
        currentSettings_,
        hasState_ ? &lastState_ : nullptr,
        isPaidPro,
        std::move(cb));
}

void CALLBACK TaskbarBandWindow::WinEventStatic(
    HWINEVENTHOOK, DWORD eventType, HWND hwnd, LONG idObject, LONG, DWORD, DWORD) {

    auto* self = g_bandInstance;
    if (!self) return;

    if (eventType == EVENT_SYSTEM_FOREGROUND) {
        if (self->onVisualInvalidated_) self->onVisualInvalidated_();
        return;
    }

    if (eventType == EVENT_OBJECT_LOCATIONCHANGE && idObject == 0) {
        if (!self->cachedTaskbarHwnd_) {
            self->cachedTaskbarHwnd_ = FindWindowW(L"Shell_TrayWnd", nullptr);
        }
        if (hwnd == self->cachedTaskbarHwnd_ && self->cachedTaskbarHwnd_ != nullptr) {
            if (self->onVisualInvalidated_) self->onVisualInvalidated_();
        }
    }
}

LRESULT CALLBACK TaskbarBandWindow::WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TaskbarBandWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<TaskbarBandWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<TaskbarBandWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    return self ? self->WndProc(hWnd, msg, wParam, lParam) : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT TaskbarBandWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (taskbarCreatedMsg_ != 0 && msg == taskbarCreatedMsg_) {
        RehookTaskbarLocationEvent();
        lastRenderSignature_.clear();
        lastMeasureSig_.clear();
        if (onVisualInvalidated_) onVisualInvalidated_();
        return 0;
    }

    switch (msg) {
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_MOUSEMOVE:
            if (!isTrackingLeave_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hWnd, 0};
                TrackMouseEvent(&tme);
                isTrackingLeave_ = true;
            }
            if (!isHovered_) {
                isHovered_ = true;
                if (onVisualInvalidated_) onVisualInvalidated_();
            }
            return 0;

        case WM_MOUSELEAVE:
            isTrackingLeave_ = false;
            if (isHovered_) {
                isHovered_ = false;
                if (onVisualInvalidated_) onVisualInvalidated_();
            }
            return 0;

        case WM_LBUTTONUP:
            if (onLeftClick_) onLeftClick_(currentRect_);
            return 0;

        case WM_RBUTTONUP: {
            POINT pt{};
            GetCursorPos(&pt);
            ShowContextMenu(pt.x, pt.y);
            return 0;
        }

        case WM_SETTINGCHANGE:
        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED:
            lastRenderSignature_.clear();
            if (onVisualInvalidated_) onVisualInvalidated_();
            return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace vakit
