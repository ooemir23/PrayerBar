#include "taskbar_band.h"
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
constexpr int kCmdToggleFlyout = 1001;
constexpr int kCmdRefresh = 1002;
constexpr int kCmdOpenSettings = 1003;
constexpr int kCmdExit = 1004;
constexpr int kCmdStopSound = 1005;
constexpr int kCmdOpenImsakiye = 1006;
constexpr int kCmdOpenPro = 1009;
constexpr int kCmdMute1h = 1010;
constexpr int kCmdMuteToday = 1011;
constexpr int kCmdUnmute = 1012;
constexpr int kCmdTestNotif = 1013;
constexpr int kCmdChangeLocation = 1015;

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

void TaskbarBandWindow::SetMenuActionCallbacks(
    std::function<void(const std::function<void(AppSettings&)>&)> onMutateSettings,
    std::function<void(int)> onOpenSettingsTab,
    std::function<void()> onTestNotification) {
    onMutateSettings_ = std::move(onMutateSettings);
    onOpenSettingsTab_ = std::move(onOpenSettingsTab);
    onTestNotification_ = std::move(onTestNotification);
}

void TaskbarBandWindow::ShowContextMenu(int screenX, int screenY) {
    ApplyWin11AppDarkMode(currentSettings_.themeMode);

    HMENU menu = CreatePopupMenu();
    HMENU notifSub = CreatePopupMenu();
    if (!menu || !notifSub) {
        if (notifSub) DestroyMenu(notifSub);
        if (menu) DestroyMenu(menu);
        return;
    }

    const AppSettings& s = currentSettings_;
    AppLanguage lang = s.language;
    auto add = [](HMENU m, UINT id, const std::wstring& text, bool checked = false, bool enabled = true) {
        AppendMenuW(m, MF_STRING | (checked ? MF_CHECKED : 0) | (enabled ? 0 : MF_GRAYED), id, text.c_str());
    };
    auto sep = [](HMENU m) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); };
    auto fmt = [](const wchar_t* pattern, const std::wstring& arg) {
        wchar_t buf[512];
        swprintf_s(buf, pattern, arg.c_str());
        return std::wstring(buf);
    };

    // 1) Status header (default item) + active audio stop
    if (hasState_) {
        TimeHM nextHm{lastState_.nextPrayerDateTime.hour, lastState_.nextPrayerDateTime.minute};
        std::wstring header = std::wstring(Tr(lang, StrId::BadgeNext)) + L": " +
                              GetEffectivePrayerName(lastState_.nextPrayer, s) + L"  " + nextHm.FormatHHMM() +
                              L"  ·  " + FormatRemainingLong(lastState_.remainingSeconds, false, lang);
        add(menu, kCmdToggleFlyout, header);
    } else {
        add(menu, kCmdToggleFlyout, Tr(lang, StrId::ToggleFlyout));
    }
    SetMenuDefaultItem(menu, kCmdToggleFlyout, FALSE);
    if (AudioPlayer::IsPlaying()) {
        add(menu, kCmdStopSound, Tr(lang, StrId::StopSoundMenu));
    }
    sep(menu);

    // 2) Core daily actions
    add(menu, kCmdOpenImsakiye, Tr(lang, StrId::OpenImsakiye));
    add(menu, kCmdChangeLocation, fmt(Tr(lang, StrId::MenuChangeLocation), s.cityName));
    add(menu, kCmdRefresh, Tr(lang, StrId::RefreshNow));
    sep(menu);

    // 3) Single-level Notifications submenu
    LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
    int nowMin = static_cast<int>(now.ToEpochSeconds() / 60);
    bool muted = s.notifyMuteUntilMin > nowMin;
    if (muted) {
        int64_t untilSec = static_cast<int64_t>(s.notifyMuteUntilMin) * 60;
        int dayMin = static_cast<int>((untilSec / 60) % (24 * 60));
        TimeHM untilHm{dayMin / 60, dayMin % 60};
        add(notifSub, 0, fmt(Tr(lang, StrId::MenuMutedUntil), untilHm.FormatHHMM()), true, false);
        add(notifSub, kCmdUnmute, Tr(lang, StrId::MenuUnmute));
        sep(notifSub);
    }
    add(notifSub, kCmdMute1h, Tr(lang, StrId::MenuMute1h));
    add(notifSub, kCmdMuteToday, Tr(lang, StrId::MenuMuteToday));
    sep(notifSub);
    add(notifSub, kCmdTestNotif, Tr(lang, StrId::BtnTestNotif));
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(notifSub), Tr(lang, StrId::MenuNotifications));

    // 4) Settings (all appearance & style controls live here with live preview), Pro, Exit
    add(menu, kCmdOpenSettings, Tr(lang, StrId::SettingsEllipsis));
    bool isPaidPro = isPaidProFn_ ? isPaidProFn_() : false;
    if (!isPaidPro) {
        add(menu, kCmdOpenPro, Tr(lang, StrId::MenuUpgradePro));
    }
    sep(menu);
    add(menu, kCmdExit, Tr(lang, StrId::ExitApp));

    SetForegroundWindow(hwnd_);
    UINT align = TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN |
                 (IsRtlLanguage(lang) ? TPM_LAYOUTRTL : 0);
    int cmd = TrackPopupMenuEx(menu, align, screenX, screenY, hwnd_, nullptr);

    auto mutate = [this](const std::function<void(AppSettings&)>& f) {
        if (onMutateSettings_) onMutateSettings_(f);
    };

    if (cmd == kCmdStopSound) {
        AudioPlayer::Stop();
    } else if (cmd == kCmdToggleFlyout) {
        if (onLeftClick_) onLeftClick_(currentRect_);
    } else if (cmd == kCmdOpenImsakiye) {
        if (onOpenImsakiye_) onOpenImsakiye_();
    } else if (cmd == kCmdOpenPro) {
        if (onOpenPro_) onOpenPro_();
    } else if (cmd == kCmdRefresh) {
        if (onRefresh_) onRefresh_();
    } else if (cmd == kCmdOpenSettings) {
        if (onOpenSettings_) onOpenSettings_();
    } else if (cmd == kCmdExit) {
        if (onExit_) onExit_();
    } else if (cmd == kCmdMute1h) {
        mutate([nowMin](AppSettings& a) { a.notifyMuteUntilMin = nowMin + 60; });
    } else if (cmd == kCmdMuteToday) {
        int minsLeftToday = 24 * 60 - (now.hour * 60 + now.minute);
        mutate([nowMin, minsLeftToday](AppSettings& a) { a.notifyMuteUntilMin = nowMin + minsLeftToday; });
    } else if (cmd == kCmdUnmute) {
        mutate([](AppSettings& a) { a.notifyMuteUntilMin = 0; });
    } else if (cmd == kCmdTestNotif) {
        if (onTestNotification_) onTestNotification_();
    } else if (cmd == kCmdChangeLocation) {
        if (onOpenSettingsTab_) onOpenSettingsTab_(1);
    }

    DestroyMenu(notifSub);
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
