#include "taskbar_band.h"
#include "taskbar_renderer.h"
#include "../core/formatter.h"
#include "../core/i18n.h"
#include "../platform/audio_player.h"
#include "../platform/taskbar_geom.h"
#include "../platform/theme_utils.h"

#include <cmath>
#include <cstdio>

namespace vakit {

namespace {

constexpr wchar_t kBandClassName[] = L"VakitCubuguTaskbarBand";
constexpr int kCmdToggleFlyout = 1001;
constexpr int kCmdRefresh = 1002;
constexpr int kCmdOpenSettings = 1003;
constexpr int kCmdExit = 1004;
constexpr int kCmdStopSound = 1005;
constexpr int kCmdOpenImsakiye = 1006;
constexpr int kCmdToggleTray = 1007;
constexpr int kCmdTogglePill = 1008;
constexpr int kCmdOpenPro = 1009;
constexpr int kCmdStyleBase = 2000;
constexpr int kCmdFlyoutBase = 2100;

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
        L"VakitÇubuğu",
        WS_POPUP,
        0, 0, 110, 40,
        nullptr, nullptr, hInst, this);

    cachedTaskbarHwnd_ = FindWindowW(L"Shell_TrayWnd", nullptr);

    hookForeground_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        WinEventStatic,
        0, 0,
        WINEVENT_OUTOFCONTEXT);

    hookLocation_ = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE,
        EVENT_OBJECT_LOCATIONCHANGE,
        nullptr,
        WinEventStatic,
        0, 0,
        WINEVENT_OUTOFCONTEXT);
}

TaskbarBandWindow::~TaskbarBandWindow() {
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

void TaskbarBandWindow::Update(const PrayerCountdownState& state, const AppSettings& settings, bool forceRedraw) {
    if (!hwnd_) return;
    currentSettings_ = settings;

    if (!settings.showTaskbarPill) {
        if (IsWindowVisible(hwnd_)) {
            ShowWindow(hwnd_, SW_HIDE);
        }
        return;
    }

    double scale = GetTaskbarScale();
    FormattedTaskbarDisplay display = FormatTaskbar(state, settings);
    int desiredWidth = MeasureDesiredPillWidth(display, settings, scale);
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
    swprintf_s(sigBuf, L"%d|%d|%d|%d|%ls|%ls|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d",
               placement.x, placement.y, placement.width, placement.height,
               display.line1.c_str(), display.line2.c_str(),
               display.showPhaseIcon ? 1 : 0, display.showDot ? 1 : 0,
               static_cast<int>(display.nextPrayer), display.isUrgent ? 1 : 0,
               progressBucket, isHovered_ ? 1 : 0, isFlyoutOpen_ ? 1 : 0,
               isLight ? 1 : 0, useOpaque ? 1 : 0, static_cast<int>(settings.fontSize),
               settings.useSystemAccentColor ? 1 : 0, static_cast<int>(settings.language));
    std::wstring sig(sigBuf);

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

    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        placement.x,
        placement.y,
        placement.width,
        placement.height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void TaskbarBandWindow::ShowContextMenu(int screenX, int screenY) {
    ApplyWin11AppDarkMode(currentSettings_.themeMode);

    HMENU menu = CreatePopupMenu();
    HMENU styleSub = CreatePopupMenu();
    HMENU flyoutSub = CreatePopupMenu();
    if (!menu || !styleSub || !flyoutSub) {
        if (menu) DestroyMenu(menu);
        if (styleSub) DestroyMenu(styleSub);
        if (flyoutSub) DestroyMenu(flyoutSub);
        return;
    }

    AppLanguage lang = currentSettings_.language;

    if (AudioPlayer::IsPlaying()) {
        AppendMenuW(menu, MF_STRING, kCmdStopSound, Tr(lang, StrId::StopSoundMenu));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }

    for (int i = 0; i <= 8; ++i) {
        auto st = static_cast<TaskbarDisplayStyle>(i);
        UINT flags = MF_STRING | (currentSettings_.displayStyle == st ? MF_CHECKED : 0);
        AppendMenuW(styleSub, flags, static_cast<UINT_PTR>(kCmdStyleBase + i), GetStyleFriendlyNameLocalized(st, lang));
    }

    for (int i = 0; i <= 3; ++i) {
        auto fd = static_cast<FlyoutDesignStyle>(i);
        UINT flags = MF_STRING | (currentSettings_.flyoutDesign == fd ? MF_CHECKED : 0);
        AppendMenuW(flyoutSub, flags, static_cast<UINT_PTR>(kCmdFlyoutBase + i), GetFlyoutDesignFriendlyName(fd, lang));
    }

    AppendMenuW(menu, MF_STRING, kCmdToggleFlyout, Tr(lang, StrId::ToggleFlyout));
    AppendMenuW(menu, MF_STRING, kCmdOpenImsakiye, Tr(lang, StrId::OpenImsakiye));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(styleSub), Tr(lang, StrId::TaskbarStyleMenu));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(flyoutSub), Tr(lang, StrId::FlyoutDesignMenu));

    UINT trayFlags = MF_STRING | (currentSettings_.showTrayIcon ? MF_CHECKED : 0) |
                     (!currentSettings_.showTaskbarPill ? MF_GRAYED : 0);
    UINT pillFlags = MF_STRING | (currentSettings_.showTaskbarPill ? MF_CHECKED : 0) |
                     (!currentSettings_.showTrayIcon ? MF_GRAYED : 0);
    AppendMenuW(menu, trayFlags, kCmdToggleTray, Tr(lang, StrId::ShowInTrayMenu));
    AppendMenuW(menu, pillFlags, kCmdTogglePill, Tr(lang, StrId::ShowInTaskbarMenu));

    bool isPaidPro = isPaidProFn_ ? isPaidProFn_() : false;
    AppendMenuW(menu, MF_STRING, kCmdOpenPro, Tr(lang, isPaidPro ? StrId::MenuManageLicense : StrId::MenuUpgradePro));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdRefresh, Tr(lang, StrId::RefreshNow));
    AppendMenuW(menu, MF_STRING, kCmdOpenSettings, Tr(lang, StrId::SettingsEllipsis));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdExit, Tr(lang, StrId::ExitApp));

    SetForegroundWindow(hwnd_);
    int cmd = TrackPopupMenuEx(
        menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
        screenX,
        screenY,
        hwnd_,
        nullptr);

    if (cmd == kCmdStopSound) {
        AudioPlayer::Stop();
    } else if (cmd == kCmdToggleFlyout) {
        if (onLeftClick_) onLeftClick_(currentRect_);
    } else if (cmd == kCmdOpenImsakiye) {
        if (onOpenImsakiye_) onOpenImsakiye_();
    } else if (cmd == kCmdOpenPro) {
        if (onOpenPro_) onOpenPro_();
    } else if (cmd == kCmdToggleTray) {
        if (onToggleTrayIcon_) onToggleTrayIcon_(!currentSettings_.showTrayIcon);
    } else if (cmd == kCmdTogglePill) {
        if (onToggleTaskbarPill_) onToggleTaskbarPill_(!currentSettings_.showTaskbarPill);
    } else if (cmd == kCmdRefresh) {
        if (onRefresh_) onRefresh_();
    } else if (cmd == kCmdOpenSettings) {
        if (onOpenSettings_) onOpenSettings_();
    } else if (cmd == kCmdExit) {
        if (onExit_) onExit_();
    } else if (cmd >= kCmdStyleBase && cmd < kCmdStyleBase + 20) {
        if (onQuickStyle_) onQuickStyle_(static_cast<TaskbarDisplayStyle>(cmd - kCmdStyleBase));
    } else if (cmd >= kCmdFlyoutBase && cmd < kCmdFlyoutBase + 10) {
        if (onQuickFlyoutDesign_) onQuickFlyoutDesign_(static_cast<FlyoutDesignStyle>(cmd - kCmdFlyoutBase));
    }

    DestroyMenu(flyoutSub);
    DestroyMenu(styleSub);
    DestroyMenu(menu);
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
        cachedTaskbarHwnd_ = FindWindowW(L"Shell_TrayWnd", nullptr);
        lastRenderSignature_.clear();
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
