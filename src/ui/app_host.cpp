#include "app_host.h"
#include "gdiplus_flat.h"
#include "../core/formatter.h"
#include "../core/i18n.h"
#include "../core/utf_util.h"
#include "../platform/audio_player.h"
#include "../platform/theme_utils.h"

#include <algorithm>
#include <cstdio>
#include <thread>

namespace vakit {

namespace {

constexpr UINT_PTR kTimerTickId = 1;
constexpr UINT_PTR kTimerWatchdogId = 2;
constexpr UINT_PTR kTimerAutoMemId = 3;
constexpr UINT_PTR kTimerTrayNotifCleanupId = 4;

RECT FallbackAnchorNearCursor() {
    POINT pt{};
    GetCursorPos(&pt);
    return {pt.x - 40, pt.y - 40, pt.x + 40, pt.y};
}

} // namespace

AppHost::AppHost(SettingsStore& settingsStore, ScheduleService& scheduleService)
    : settingsStore_(settingsStore),
      scheduleService_(scheduleService),
      settings_(settingsStore.Load()) {

    taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");
    scheduleService_.SetRefreshedCallback([this]() {
        if (hostHwnd_) {
            PostMessageW(hostHwnd_, kMsgScheduleRefreshed, 0, 0);
        }
    });
}

AppHost::~AppHost() {
    AudioPlayer::Stop();
    settingsWindow_.reset();
    proWindow_.reset();
    imsakiyeWindow_.reset();
    flyoutWindow_.reset();
    trayHost_.reset();
    bandWindow_.reset();

    if (hostHwnd_) {
        KillTimer(hostHwnd_, kTimerTickId);
        KillTimer(hostHwnd_, kTimerWatchdogId);
        KillTimer(hostHwnd_, kTimerAutoMemId);
        KillTimer(hostHwnd_, kTimerTrayNotifCleanupId);
        DestroyWindow(hostHwnd_);
        hostHwnd_ = nullptr;
    }
    gdip::Shutdown();
}

int AppHost::Run() {
    ApplyWin11AppDarkMode(settings_.themeMode);

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HostWndProcStatic;
    wc.hInstance = hInst;
    wc.lpszClassName = kHostWindowClassName;
    RegisterClassExW(&wc);

    hostHwnd_ = CreateWindowExW(
        0, kHostWindowClassName, L"VakitCubuguHost", 0,
        0, 0, 0, 0, nullptr, nullptr, hInst, this);
    if (!hostHwnd_) return 1;

    bandWindow_ = std::make_unique<TaskbarBandWindow>();
    trayHost_ = std::make_unique<TrayIconHost>(hostHwnd_);

    proWindow_ = std::make_unique<ProWindow>(
        settingsStore_,
        licenseManager_,
        [this]() {
            RefreshTick(false);
            if (flyoutWindow_) flyoutWindow_->RefreshView();
            if (imsakiyeWindow_) imsakiyeWindow_->RefreshIfOpen();
            if (settingsWindow_) settingsWindow_->RefreshLicenseStatus();
        });

    imsakiyeWindow_ = std::make_unique<ImsakiyeWindow>(
        settingsStore_,
        scheduleService_,
        licenseManager_,
        [this]() { if (proWindow_) proWindow_->OpenOrFocus(true); });

    flyoutWindow_ = std::make_unique<FlyoutWindow>(
        settingsStore_,
        scheduleService_,
        [this]() { settingsWindow_->OpenOrFocus(); },
        [this]() { imsakiyeWindow_->OpenOrFocus(); },
        [this](bool isOpen) {
            if (bandWindow_) {
                auto st = scheduleService_.ComputeState(ScheduleService::GetCurrentLocalDateTime(), settings_);
                bandWindow_->SetFlyoutOpenState(isOpen, &st, settings_);
            }
        },
        [this]() {
            scheduleService_.ForceRefreshMonth(ScheduleService::GetCurrentLocalDateTime().date, settings_);
        });

    settingsWindow_ = std::make_unique<SettingsWindow>(
        settingsStore_,
        scheduleService_,
        licenseManager_,
        [this]() {
            settings_ = settingsStore_.Load();
            ApplyWin11AppDarkMode(settings_.themeMode);
            RefreshTick(false);
            ScheduleNextTickTimer();
            if (flyoutWindow_) flyoutWindow_->RefreshView();
            if (imsakiyeWindow_) imsakiyeWindow_->RefreshIfOpen();
            if (proWindow_) proWindow_->RefreshThemeOrLanguage();
        },
        [this](const std::wstring& title, const std::wstring& body, bool silent) {
            if (trayHost_) {
                trayHost_->ShowNotification(title, body, silent);
                if (!settings_.showTrayIcon && hostHwnd_) {
                    SetTimer(hostHwnd_, kTimerTrayNotifCleanupId, 15000, nullptr);
                }
            }
        },
        [this]() { imsakiyeWindow_->OpenOrFocus(); },
        [this](bool lockedHint) { if (proWindow_) proWindow_->OpenOrFocus(lockedHint); });

    bandWindow_->SetCallbacks(
        [this](RECT anchor) { ToggleFlyout(anchor); },
        [this]() { TriggerBackgroundRefresh(); },
        [this]() { settingsWindow_->OpenOrFocus(); },
        [this](TaskbarDisplayStyle style) {
            if (LicenseManager::IsFeatureProOnly(style) && !licenseManager_.IsProActive()) {
                if (proWindow_) proWindow_->OpenOrFocus(true);
                return;
            }
            settings_.displayStyle = style;
            settingsStore_.Save(settings_);
            RefreshTick(false);
        },
        []() { PostQuitMessage(0); },
        [this]() { RefreshTick(false); });

    bandWindow_->SetExtraMenuCallbacks(
        [this]() { imsakiyeWindow_->OpenOrFocus(); },
        [this](FlyoutDesignStyle fd) {
            if (LicenseManager::IsFeatureProOnly(fd) && !licenseManager_.IsProActive()) {
                if (proWindow_) proWindow_->OpenOrFocus(true);
                return;
            }
            settings_.flyoutDesign = fd;
            settingsStore_.Save(settings_);
            if (flyoutWindow_) flyoutWindow_->RefreshView();
        },
        [this](bool wantTray) {
            if (!wantTray && !settings_.showTaskbarPill) return;
            settings_.showTrayIcon = wantTray;
            settingsStore_.Save(settings_);
            RefreshTick(false);
            if (settingsWindow_ && settingsWindow_->IsOpen()) {
                PostMessageW(hostHwnd_, kMsgScheduleRefreshed, 0, 0);
            }
        },
        [this](bool wantPill) {
            if (!wantPill && !settings_.showTrayIcon) return;
            settings_.showTaskbarPill = wantPill;
            settingsStore_.Save(settings_);
            RefreshTick(false);
        },
        [this]() { if (proWindow_) proWindow_->OpenOrFocus(false); },
        [this]() -> bool {
            auto t = licenseManager_.GetStatus().tier;
            return t == LicenseTier::ProAnnual || t == LicenseTier::ProLifetime;
        });

    RefreshTick(false);
    ScheduleNextTickTimer();
    SetTimer(hostHwnd_, kTimerWatchdogId, 5000, nullptr);
    SetTimer(hostHwnd_, kTimerAutoMemId, 600000, nullptr);

    if (IsPackagedApp()) {
        HWND host = hostHwnd_;
        std::thread([host]() {
            auto snap = QueryMicrosoftStoreLicenseSync();
            auto* ptr = new StoreLicenseSnapshot(snap);
            if (!PostMessageW(host, kMsgStoreLicenseSynced, 0, reinterpret_cast<LPARAM>(ptr))) {
                delete ptr;
            }
        }).detach();
    }

    if (settings_.autoOptimizeMemory) {
        TrimWorkingSet();
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

void AppHost::ToggleFlyout(RECT anchorRect) {
    if (!flyoutWindow_) return;
    if (flyoutWindow_->IsOpen()) {
        flyoutWindow_->CloseNow();
        return;
    }
    if ((GetTickCount64() - flyoutWindow_->LastClosedTick()) < 220) {
        return;
    }
    flyoutWindow_->ShowRightDocked(anchorRect);
}

void AppHost::TriggerBackgroundRefresh() {
    AppSettings copy = settings_;
    ScheduleService* svc = &scheduleService_;
    std::thread([svc, copy]() {
        svc->ForceRefreshMonth(ScheduleService::GetCurrentLocalDateTime().date, copy);
    }).detach();
}

void AppHost::RefreshTick(bool checkNotifications) {
    LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
    PrayerCountdownState state = scheduleService_.ComputeState(now, settings_);
    if (bandWindow_) bandWindow_->Update(state, settings_);
    if (trayHost_) trayHost_->Update(state, settings_);
    if (checkNotifications) {
        EvaluateNotifications(now, state);
    }
}

void AppHost::ScheduleNextTickTimer() {
    if (!hostHwnd_) return;
    SYSTEMTIME st{};
    GetLocalTime(&st);

    UINT delayMs = 0;
    if (settings_.showSeconds) {
        delayMs = static_cast<UINT>(std::clamp(1000 - static_cast<int>(st.wMilliseconds) + 25, 100, 1050));
    } else {
        delayMs = static_cast<UINT>(std::clamp((60 - static_cast<int>(st.wSecond)) * 1000 - static_cast<int>(st.wMilliseconds) + 60, 500, 61000));
    }
    SetTimer(hostHwnd_, kTimerTickId, delayMs, nullptr);
}

void AppHost::EvaluateNotifications(const LocalDateTime& now, const PrayerCountdownState& state) {
    if (sentNotifications_.size() > 64) {
        sentNotifications_.clear();
    }

    bool quietActive = settings_.quietDuringFullscreen && IsQuietOrFullscreenModeActive();

    DateYMD today = now.date;
    DateYMD tomorrow = today.AddDays(1);

    struct Candidate {
        DateYMD date;
        PrayerKind kind;
        LocalDateTime entryDt;
    };
    Candidate candidates[7];
    auto entries = state.today.ToOrderedEntries();
    for (size_t i = 0; i < 6; ++i) {
        candidates[i] = {today, entries[i].first, LocalDateTime::FromDateTime(today, entries[i].second)};
    }
    candidates[6] = {tomorrow, PrayerKind::Imsak, LocalDateTime::FromDateTime(tomorrow, state.tomorrowImsak)};

    int64_t nowSec = now.ToEpochSeconds();

    auto dispatchNotif = [&](const std::wstring& title, const std::wstring& body, NotificationSoundKind sound, bool playSoundRule) {
        bool playAudio = !quietActive && playSoundRule && (sound != NotificationSoundKind::None);
        if (trayHost_) {
            trayHost_->ShowNotification(title, body, quietActive || playAudio || !playSoundRule);
            if (!settings_.showTrayIcon && hostHwnd_) {
                SetTimer(hostHwnd_, kTimerTrayNotifCleanupId, 15000, nullptr);
            }
        }
        if (playAudio) {
            AudioPlayer::Play(sound, settings_.customSoundPath, settings_.soundVolumePercent);
        }
    };

    for (const auto& c : candidates) {
        const PrayerAlertRule& rule = settings_.alerts.GetRule(c.kind);
        int64_t entrySec = c.entryDt.ToEpochSeconds();
        int64_t elapsedSec = nowSec - entrySec;

        if (rule.notifyAtTime && elapsedSec >= 0 && elapsedSec < 65) {
            wchar_t keyBuf[64];
            swprintf_s(keyBuf, L"%04d%02d%02d_%d_enter", c.date.year, c.date.month, c.date.day, static_cast<int>(c.kind));
            if (sentNotifications_.insert(keyBuf).second) {
                TimeHM hm{c.entryDt.hour, c.entryDt.minute};
                std::wstring hhmm = hm.FormatHHMM();
                dispatchNotif(
                    FormatNotificationAtTimeTitle(c.kind, settings_),
                    FormatNotificationAtTimeBody(settings_.cityName, c.kind, hhmm, settings_),
                    settings_.entrySound,
                    rule.playSound);
            }
        }

        if (rule.preNotifyMinutes > 0) {
            double remMin = static_cast<double>(entrySec - nowSec) / 60.0;
            if (remMin <= static_cast<double>(rule.preNotifyMinutes) &&
                remMin > static_cast<double>(rule.preNotifyMinutes) - 1.05) {
                wchar_t keyBuf[64];
                swprintf_s(keyBuf, L"%04d%02d%02d_%d_pre%d",
                           c.date.year, c.date.month, c.date.day, static_cast<int>(c.kind), rule.preNotifyMinutes);
                if (sentNotifications_.insert(keyBuf).second) {
                    TimeHM hm{c.entryDt.hour, c.entryDt.minute};
                    std::wstring hhmm = hm.FormatHHMM();
                    dispatchNotif(
                        FormatNotificationPreTitle(c.kind, rule.preNotifyMinutes, settings_),
                        FormatNotificationPreBody(c.kind, hhmm, settings_.cityName, settings_),
                        settings_.preNotifySound,
                        rule.playSound);
                }
            }
        }
    }
}

LRESULT CALLBACK AppHost::HostWndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppHost* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<AppHost*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<AppHost*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    return self ? self->HostWndProc(hWnd, msg, wParam, lParam) : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT AppHost::HostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (taskbarCreatedMsg_ != 0 && msg == taskbarCreatedMsg_) {
        auto state = scheduleService_.ComputeState(ScheduleService::GetCurrentLocalDateTime(), settings_);
        if (trayHost_) trayHost_->RecreateIfNeeded(state, settings_);
        if (bandWindow_) bandWindow_->Update(state, settings_, true);
        return 0;
    }

    switch (msg) {
        case WM_TIMER:
            if (wParam == kTimerTickId) {
                RefreshTick(true);
                ScheduleNextTickTimer();
            } else if (wParam == kTimerWatchdogId) {
                auto state = scheduleService_.ComputeState(ScheduleService::GetCurrentLocalDateTime(), settings_);
                if (bandWindow_) bandWindow_->Update(state, settings_, false);
            } else if (wParam == kTimerAutoMemId) {
                bool anyWindowOpen = (flyoutWindow_ && flyoutWindow_->IsOpen()) ||
                                     (settingsWindow_ && settingsWindow_->IsOpen()) ||
                                     (imsakiyeWindow_ && imsakiyeWindow_->IsOpen()) ||
                                     (proWindow_ && proWindow_->IsOpen());
                if (settings_.autoOptimizeMemory && !anyWindowOpen) {
                    scheduleService_.TrimMemoryCache(ScheduleService::GetCurrentLocalDateTime().date);
                    TrimWorkingSet();
                }
            } else if (wParam == kTimerTrayNotifCleanupId) {
                KillTimer(hWnd, kTimerTrayNotifCleanupId);
                if (trayHost_) trayHost_->CleanupTemporaryHiddenIcon();
            }
            return 0;

        case TrayIconHost::kCallbackMessageId: {
            UINT mouseMsg = static_cast<UINT>(lParam & 0xFFFF);
            if (mouseMsg == WM_LBUTTONUP) {
                RECT anchor = bandWindow_ ? bandWindow_->CurrentRect() : FallbackAnchorNearCursor();
                ToggleFlyout(anchor);
            } else if (mouseMsg == WM_RBUTTONUP) {
                POINT pt{};
                GetCursorPos(&pt);
                if (bandWindow_) bandWindow_->ShowContextMenu(pt.x, pt.y);
            }
            return 0;
        }

        case kMsgShowFlyout:
            ToggleFlyout(bandWindow_ ? bandWindow_->CurrentRect() : FallbackAnchorNearCursor());
            return 0;

        case kMsgScheduleRefreshed:
            RefreshTick(false);
            if (flyoutWindow_) flyoutWindow_->RefreshView();
            if (imsakiyeWindow_) imsakiyeWindow_->RefreshIfOpen();
            if (settingsWindow_) settingsWindow_->RefreshPreviewAndMemory();
            return 0;

        case kMsgStoreLicenseSynced: {
            auto* snap = reinterpret_cast<StoreLicenseSnapshot*>(lParam);
            if (snap) {
                bool changed = licenseManager_.ApplyStoreLicenseSnapshot(*snap, LicenseManager::CurrentLocalDate());
                delete snap;
                if (changed) {
                    RefreshTick(false);
                    if (flyoutWindow_) flyoutWindow_->RefreshView();
                    if (imsakiyeWindow_) imsakiyeWindow_->RefreshIfOpen();
                    if (settingsWindow_) settingsWindow_->RefreshLicenseStatus();
                    if (proWindow_) proWindow_->RefreshThemeOrLanguage();
                }
            }
            return 0;
        }

        case WM_POWERBROADCAST:
            if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMSUSPEND || wParam == PBT_APMPOWERSTATUSCHANGE) {
                RefreshTick(false);
                ScheduleNextTickTimer();
            }
            return TRUE;

        case WM_SETTINGCHANGE:
            ApplyWin11AppDarkMode(settings_.themeMode);
            RefreshTick(false);
            if (flyoutWindow_) flyoutWindow_->RefreshView();
            if (imsakiyeWindow_) imsakiyeWindow_->RefreshIfOpen();
            if (proWindow_) proWindow_->RefreshThemeOrLanguage();
            return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace vakit
