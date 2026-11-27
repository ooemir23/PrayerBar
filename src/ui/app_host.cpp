#include "app_host.h"
#include "gdiplus_flat.h"
#include "../core/commercial_config.h"
#include "../core/formatter.h"
#include "../core/i18n.h"
#include "../core/utf_util.h"
#include "../platform/audio_player.h"
#include "../platform/theme_utils.h"
#include "../platform/toast.h"
#include "../platform/widget_provider.h"
#include "../platform/win_store.h"
#include "../core/prayer_calc.h"

#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <thread>

namespace vakit {

namespace {

constexpr int kBalloonNone = 0;
constexpr int kBalloonReview = 1;
constexpr int kBalloonLocation = 2;
constexpr int kHotkeyId = 1;
constexpr UINT_PTR kTimerTickId = 1;
constexpr UINT_PTR kTimerWatchdogId = 2;
constexpr UINT_PTR kTimerAutoMemId = 3;
constexpr UINT_PTR kTimerTrayNotifCleanupId = 4;
constexpr UINT_PTR kTimerAutoLocRetryId = 5;
constexpr UINT_PTR kTimerStoreUpdateCheckId = 6;

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
}

AppHost::~AppHost() {
    shuttingDown_ = true;
    UnregisterWidgetProvider();
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
        KillTimer(hostHwnd_, kTimerAutoLocRetryId);
        KillTimer(hostHwnd_, kTimerStoreUpdateCheckId);
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
        0, kHostWindowClassName, L"PrayerBarHost", 0,
        0, 0, 0, 0, nullptr, nullptr, hInst, this);
    if (!hostHwnd_) return 1;

    if (HMODULE k32 = GetModuleHandleW(L"kernel32.dll")) {
        using RegRestartFn = HRESULT(WINAPI*)(PCWSTR, DWORD);
        if (auto fn = reinterpret_cast<RegRestartFn>(reinterpret_cast<void*>(GetProcAddress(k32, "RegisterApplicationRestart")))) {
            fn(L"--restarted", 1 /* RESTART_NO_CRASH */ | 2 /* RESTART_NO_HANG */);
        }
    }

    // Capture the HWND by value: background fetch threads may fire after AppHost is gone.
    scheduleService_.SetRefreshedCallback([h = hostHwnd_]() { PostMessageW(h, kMsgScheduleRefreshed, 0, 0); });

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
        },
        [this]() {
            settings_ = settingsStore_.Load();
            if (settingsWindow_ && settingsWindow_->IsOpen()) {
                settingsWindow_->SyncNotificationControls();
                settingsWindow_->RefreshPreviewAndMemory();
            }
        });

    settingsWindow_ = std::make_unique<SettingsWindow>(
        settingsStore_,
        scheduleService_,
        licenseManager_,
        [this]() {
            settings_ = settingsStore_.Load();
            OnSettingsChanged();
        },
        [this](const std::wstring& title, const std::wstring& body, bool hasAudio) {
            ShowUserNotification(title, body, true, hasAudio);
        },
        [this]() { imsakiyeWindow_->OpenOrFocus(); },
        [this](bool lockedHint) { if (proWindow_) proWindow_->OpenOrFocus(lockedHint); },
        [this]() { return hotkeyOk_; },
        [this]() {
            RefreshTick(false);
            if (flyoutWindow_) flyoutWindow_->RefreshView();
            if (imsakiyeWindow_) imsakiyeWindow_->RefreshIfOpen();
            if (proWindow_) proWindow_->RefreshThemeOrLanguage();
        });

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
        [this]() { RefreshBandOnly(true); });

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

    bandWindow_->SetMenuActionCallbacks(
        [this](const std::function<void(AppSettings&)>& change) {
            change(settings_);
            settingsStore_.Save(settings_);
            OnSettingsChanged();
            if (settingsWindow_ && settingsWindow_->IsOpen()) {
                settingsWindow_->SyncNotificationControls();
                settingsWindow_->RefreshPreviewAndMemory();
            }
        },
        [this](int tab) { if (settingsWindow_) settingsWindow_->OpenOrFocus(tab); },
        [this]() {
            bool playAudio = settings_.preNotifySound != NotificationSoundKind::None;
            if (playAudio) {
                AudioPlayer::Play(settings_.preNotifySound, settings_.customSoundPath, settings_.soundVolumePercent);
                RefreshTick(false);
            }
            ShowUserNotification(Tr(settings_.language, StrId::TestNotifTitle), Tr(settings_.language, StrId::TestNotifBody),
                                 true, playAudio);
        });

    ApplyHotkey();
    CheckTimeZoneChange();
    RefreshTick(false);
    if (settings_.autoDetectLocation) {
        StartAutomaticLocationDetection();
    }
    ScheduleNextTickTimer();
    watchdogIntervalMs_ = 15000;
    SetTimer(hostHwnd_, kTimerWatchdogId, watchdogIntervalMs_, nullptr);
    SetTimer(hostHwnd_, kTimerAutoMemId, 600000, nullptr);

    if (IsPackagedApp()) {
        std::thread([startWithWindows = settings_.startWithWindows]() {
            MigrateLegacyPackagedStartupIfNeeded(startWithWindows);
        }).detach();

        SetTimer(hostHwnd_, kTimerStoreUpdateCheckId, 25000, nullptr);

        RegisterWidgetProvider(
            [this]() { return FormatWidgetDataJson(scheduleService_.ComputeState(ScheduleService::GetCurrentLocalDateTime(), settings_), settings_); },
            [h = hostHwnd_]() { PostMessageW(h, kMsgShowFlyout, 0, 0); });

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

void AppHost::OnSettingsChanged() {
    if (!settings_.autoDetectLocation) {
        ++autoLocGeneration_;
        autoLocRetryCount_ = 0;
        if (hostHwnd_) KillTimer(hostHwnd_, kTimerAutoLocRetryId);
    }
    ApplyWin11AppDarkMode(settings_.themeMode);
    ApplyHotkey();
    RefreshTick(false);
    ScheduleNextTickTimer();
    if (flyoutWindow_) flyoutWindow_->RefreshView();
    if (imsakiyeWindow_) imsakiyeWindow_->RefreshIfOpen();
    if (proWindow_) proWindow_->RefreshThemeOrLanguage();
}

void AppHost::ShowUserNotification(const std::wstring& title, const std::wstring& body, bool silent, bool hasAudio) {
    AppLanguage lang = settings_.language;
    std::wstring stopLabel = hasAudio ? Tr(lang, StrId::ToastStopAdhan) : L"";
    std::function<void()> stopCb;
    if (hasAudio) {
        HWND h = hostHwnd_;
        stopCb = [h]() {
            AudioPlayer::Stop();
            if (h) PostMessageW(h, kMsgScheduleRefreshed, 0, 0);
        };
    }
    // Toast first (Mute Adhan / Dismiss, Notification Center); classic tray balloon if toasts fail.
    if (settings_.modernToasts && ShowModernToast(title, body, silent, stopLabel, stopCb)) {
        balloonStopsAudio_ = false;
        return;
    }
    if (!trayHost_) return;
    std::wstring balloonBody = hasAudio ? (body + L"\n" + Tr(lang, StrId::BalloonStopHint)) : body;
    trayHost_->ShowNotification(title, balloonBody, silent);
    pendingBalloonAction_ = kBalloonNone;
    balloonStopsAudio_ = hasAudio;
    if (!settings_.showTrayIcon && hostHwnd_) SetTimer(hostHwnd_, kTimerTrayNotifCleanupId, 15000, nullptr);
}

void AppHost::ShowActionBalloon(const std::wstring& title, const std::wstring& body, int action) {
    // Balloons report clicks back to this window, so actionable prompts always use them.
    if (!trayHost_) return;
    trayHost_->ShowNotification(title, body, true);
    pendingBalloonAction_ = action;
    if (!settings_.showTrayIcon && hostHwnd_) SetTimer(hostHwnd_, kTimerTrayNotifCleanupId, 15000, nullptr);
}

void AppHost::ApplyHotkey() {
    if (!hostHwnd_) return;
    UnregisterHotKey(hostHwnd_, kHotkeyId);
    hotkeyOk_ = true;
    if (settings_.globalHotkey) {
        // MOD_NOREPEAT (0x4000): one toggle per key press. Fails when another app already owns the combination.
        hotkeyOk_ = RegisterHotKey(hostHwnd_, kHotkeyId, static_cast<UINT>(settings_.hotkeyMods) | 0x4000,
                                   static_cast<UINT>(settings_.hotkeyVk)) != FALSE;
    }
}

bool AppHost::CheckTimeZoneChange() {
    DYNAMIC_TIME_ZONE_INFORMATION tz{};
    if (GetDynamicTimeZoneInformation(&tz) == TIME_ZONE_ID_INVALID) return false;
    uint32_t h = 2166136261u;  // FNV-1a of the zone key; DST switches keep the same key
    for (const wchar_t* p = tz.TimeZoneKeyName; *p; ++p) h = (h ^ static_cast<uint32_t>(*p)) * 16777619u;
    int hash = static_cast<int>(h & 0x7FFFFFFF) | 1;
    if (settings_.lastTzHash == hash) return false;
    bool firstRecord = (settings_.lastTzHash == 0);
    settings_.lastTzHash = hash;
    settingsStore_.Save(settings_);
    if (!firstRecord && settings_.travelMode) {
        AppLanguage lang = settings_.language;
        ShowActionBalloon(Tr(lang, StrId::TravelTitle), Tr(lang, StrId::TravelBody), kBalloonLocation);
    }
    return !firstRecord;
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

void AppHost::StartAutomaticLocationDetection() {
    HWND target = hostHwnd_;
    unsigned int gen = autoLocGeneration_;
    std::thread([target, gen]() {
        auto detected = ScheduleService::DetectCurrentLocation();
        auto* heapPtr = detected ? new DetectedLocationResult(*detected) : nullptr;
        if (!PostMessageW(target, kMsgAutoLocationDetected, static_cast<WPARAM>(gen), reinterpret_cast<LPARAM>(heapPtr))) {
            delete heapPtr;
        }
    }).detach();
}

void AppHost::RefreshTick(bool checkNotifications) {
    LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
    PrayerCountdownState state = scheduleService_.ComputeState(now, settings_);
    lastState_ = state;
    hasLastState_ = true;
    if (bandWindow_) bandWindow_->Update(state, settings_);
    if (trayHost_) trayHost_->Update(state, settings_);
    PushWidgetUpdate();
    if (checkNotifications) {
        EvaluateNotifications(now, state);
    }
    SyncWatchdogInterval();
}

void AppHost::RefreshBandOnly(bool raiseZOrder) {
    if (bandWindow_ && hasLastState_) {
        bandWindow_->Update(lastState_, settings_, false, raiseZOrder);
    }
}

void AppHost::SyncWatchdogInterval() {
    if (!hostHwnd_) return;
    UINT target = AudioPlayer::IsPlaying() ? 2000u : 15000u;
    if (target != watchdogIntervalMs_) {
        watchdogIntervalMs_ = target;
        SetTimer(hostHwnd_, kTimerWatchdogId, watchdogIntervalMs_, nullptr);
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

    // User muted notifications from the menu: skip everything (alerts, Jumu'ah, rating ask) until the window ends.
    if (settings_.notifyMuteUntilMin > static_cast<int>(now.ToEpochSeconds() / 60)) return;
    if (!settings_.notificationsEnabled) return;  // master switch in Settings > Notifications

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
        if (playAudio) {
            AudioPlayer::Play(sound, settings_.customSoundPath, settings_.soundVolumePercent);
            if (trayHost_) trayHost_->Update(state, settings_, false);
            SyncWatchdogInterval();
        }
        ShowUserNotification(title, body, quietActive || playAudio || !playSoundRule, playAudio);
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
                    (c.kind == PrayerKind::Imsak && settings_.fajrSound >= 0)
                        ? static_cast<NotificationSoundKind>(settings_.fajrSound)
                        : settings_.entrySound,
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

    // Jumu'ah: one reminder 45 min before Dhuhr on Fridays.
    if (settings_.fridayReminder && today.DayOfWeek() == 5) {
        int64_t dhuhrSec = LocalDateTime::FromDateTime(today, state.today.ogle).ToEpochSeconds();
        int64_t remSec = dhuhrSec - nowSec;
        if (remSec <= 45 * 60 && remSec > 44 * 60 - 3) {
            wchar_t keyBuf[32];
            swprintf_s(keyBuf, L"%04d%02d%02d_jumuah", today.year, today.month, today.day);
            if (sentNotifications_.insert(keyBuf).second) {
                AppLanguage lang = ResolveEffectiveLanguage(settings_.language);
                dispatchNotif(Tr(lang, StrId::NotifFridayTitle), Tr(lang, StrId::NotifFridayBody),
                              settings_.preNotifySound, settings_.alerts.GetRule(PrayerKind::Ogle).playSound);
            }
        }
    }

    static constexpr StrId kHolidayNames[] = {
        StrId::HolHijriNewYear, StrId::HolHijriNewYear, StrId::HolAshura, StrId::HolMawlid, StrId::HolRaghaib,
        StrId::HolMiraj, StrId::HolBarat, StrId::HolRamadanStart, StrId::HolQadr, StrId::HolEidFitr,
        StrId::HolArafah, StrId::HolEidAdha};

    // Eid prayer: heads-up 12 h before it (prayer ~45 min after sunrise, as announced by Diyanet) with the time.
    // shortcut: tabular Hijri date can be a day off from local sighting; 1 h catch-up window if the PC was asleep.
    HolidayEve eve = settings_.holidayReminders ? FindHolidayStartingTomorrow(today) : HolidayEve{};
    bool isEid = eve.id == HolidayId::EidFitr || eve.id == HolidayId::EidAdha;
    if (isEid) {
        int prayerMin = scheduleService_.GetDaySchedule(tomorrow, settings_).gunes.TotalMinutes() + 45;
        TimeHM prayerHm{prayerMin / 60, prayerMin % 60};
        int64_t remSec = LocalDateTime::FromDateTime(tomorrow, prayerHm).ToEpochSeconds() - nowSec;
        if (remSec <= 12 * 3600 && remSec > 11 * 3600) {
            wchar_t keyBuf[32];
            swprintf_s(keyBuf, L"%04d%02d%02d_eidprayer", today.year, today.month, today.day);
            if (sentNotifications_.insert(keyBuf).second) {
                AppLanguage lang = settings_.language;
                wchar_t body[256];
                swprintf_s(body, Tr(lang, StrId::EidPrayerBody), Tr(lang, kHolidayNames[static_cast<int>(eve.id)]),
                           prayerHm.FormatHHMM().c_str(), settings_.cityName.c_str());
                dispatchNotif(Tr(lang, StrId::EidPrayerTitle), body, settings_.preNotifySound, true);
            }
        }
    }

    // Islamic holidays: one reminder at 20:00 on the evening before (kandil nights are that same evening).
    // Eid has its own reminder above, so skip the generic one for it.
    int nowMinOfDay = now.hour * 60 + now.minute;
    if (settings_.holidayReminders && !isEid && nowMinOfDay >= 20 * 60 && nowMinOfDay < 20 * 60 + 2) {
        if (eve.id != HolidayId::None) {
            wchar_t keyBuf[32];
            swprintf_s(keyBuf, L"%04d%02d%02d_holiday", today.year, today.month, today.day);
            if (sentNotifications_.insert(keyBuf).second) {
                AppLanguage lang = settings_.language;
                wchar_t body[256];
                swprintf_s(body, Tr(lang, eve.isNight ? StrId::HolidayTonight : StrId::HolidayTomorrow),
                           Tr(lang, kHolidayNames[static_cast<int>(eve.id)]));
                dispatchNotif(Tr(lang, StrId::HolidayTitle), body, settings_.preNotifySound, true);
            }
        }
    }

    // One-time Store rating ask after a week of use, daytime only, never while gaming/presenting.
    if (!settings_.reviewPrompted && !quietActive && IsPackagedApp() && trayHost_ &&
        now.hour >= 10 && now.hour < 21 &&
        today.ToEpochDays() - licenseManager_.FirstLaunchEpochDays() >= 7) {
        settings_.reviewPrompted = true;
        settingsStore_.Save(settings_);
        AppLanguage lang = ResolveEffectiveLanguage(settings_.language);
        ShowActionBalloon(Tr(lang, StrId::NotifReviewTitle), Tr(lang, StrId::NotifReviewBody), kBalloonReview);
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
                lastState_ = state;
                hasLastState_ = true;
                if (bandWindow_) bandWindow_->Update(state, settings_, false, false);
                if (trayHost_) trayHost_->Update(state, settings_, false);
                SyncWatchdogInterval();
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
            } else if (wParam == kTimerAutoLocRetryId) {
                KillTimer(hWnd, kTimerAutoLocRetryId);
                if (settings_.autoDetectLocation) {
                    StartAutomaticLocationDetection();
                }
            } else if (wParam == kTimerStoreUpdateCheckId) {
                SetTimer(hWnd, kTimerStoreUpdateCheckId, 21600000u, nullptr);
                static std::atomic<bool> storeUpdateInFlight{false};
                if (!storeUpdateInFlight.exchange(true)) {
                    HWND host = hostHwnd_;
                    std::thread([host]() {
                        StoreUpdateOutcome outcome = CheckAndInstallStoreUpdatesSilently();
                        storeUpdateInFlight.store(false);
                        if (outcome == StoreUpdateOutcome::CompletedNeedsRestart && host) {
                            PostMessageW(host, kMsgStoreUpdateReady, 0, 0);
                        }
                    }).detach();
                }
            }
            return 0;

        case TrayIconHost::kCallbackMessageId: {
            UINT mouseMsg = static_cast<UINT>(lParam & 0xFFFF);
            if (mouseMsg == NIN_BALLOONUSERCLICK || mouseMsg == NIN_BALLOONHIDE || mouseMsg == NIN_BALLOONTIMEOUT) {
                if ((mouseMsg == NIN_BALLOONUSERCLICK || mouseMsg == NIN_BALLOONHIDE) && balloonStopsAudio_) {
                    AudioPlayer::Stop();
                    balloonStopsAudio_ = false;
                } else if (mouseMsg == NIN_BALLOONUSERCLICK && pendingBalloonAction_ == kBalloonReview) {
                    ShellExecuteW(hWnd, L"open", commercial::kMicrosoftStoreReviewUri, nullptr, nullptr, SW_SHOWNORMAL);
                } else if (mouseMsg == NIN_BALLOONUSERCLICK && pendingBalloonAction_ == kBalloonLocation) {
                    if (settingsWindow_) settingsWindow_->OpenOrFocus(1);
                }
                pendingBalloonAction_ = kBalloonNone;
                if (mouseMsg == NIN_BALLOONTIMEOUT) balloonStopsAudio_ = false;
                return 0;
            }
            if (mouseMsg == WM_LBUTTONUP || mouseMsg == WM_MBUTTONUP) {
                if (AudioPlayer::IsPlaying()) {
                    AudioPlayer::Stop();
                    RefreshTick(false);
                    if (flyoutWindow_) flyoutWindow_->RefreshView();
                    return 0;
                }
                if (mouseMsg == WM_LBUTTONUP) {
                    RECT anchor = bandWindow_ ? bandWindow_->CurrentRect() : FallbackAnchorNearCursor();
                    ToggleFlyout(anchor);
                }
            } else if (mouseMsg == WM_RBUTTONUP) {
                POINT pt{};
                GetCursorPos(&pt);
                if (bandWindow_) bandWindow_->ShowContextMenu(pt.x, pt.y);
            }
            return 0;
        }

        case kMsgShowSettings:
            if (settingsWindow_) settingsWindow_->OpenOrFocus(static_cast<int>(wParam));
            return 0;

        case kMsgShowFlyout:
            ToggleFlyout(bandWindow_ ? bandWindow_->CurrentRect() : FallbackAnchorNearCursor());
            return 0;

        case kMsgAutoLocationDetected: {
            auto* detected = reinterpret_cast<DetectedLocationResult*>(lParam);
            if (static_cast<unsigned int>(wParam) != autoLocGeneration_) {
                delete detected;
                return 0;
            }
            if (!detected) {
                if (settings_.autoDetectLocation && autoLocRetryCount_ < 4) {
                    static constexpr UINT kRetryDelaysMs[4] = {8000, 20000, 45000, 90000};
                    SetTimer(hWnd, kTimerAutoLocRetryId, kRetryDelaysMs[autoLocRetryCount_++], nullptr);
                }
                return 0;
            }
            autoLocRetryCount_ = 0;
            KillTimer(hWnd, kTimerAutoLocRetryId);
            if (settings_.autoDetectLocation) {
                settings_.cityId = detected->cityId;
                settings_.cityName = detected->cityName;
                settings_.countryName = detected->countryName;
                settings_.latitude = detected->latitude;
                settings_.longitude = detected->longitude;
                settings_.timeZoneId = detected->timeZoneId;
                settings_.useCustomCoordinates = false;
                if (settings_.autoSelectMethodByCountry) {
                    settings_.calculationMethod = detected->recommendedMethod;
                }
                settingsStore_.Save(settings_);
                if (settingsWindow_) settingsWindow_->ApplyDetectedLocation(*detected);
                OnSettingsChanged();
            }
            delete detected;
            return 0;
        }

        case kMsgStoreUpdateReady:
            relaunchAfterUpdate_ = true;
            PostQuitMessage(0);
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
                if (wParam == PBT_APMRESUMEAUTOMATIC && settings_.autoDetectLocation && autoLocRetryCount_ > 0) {
                    StartAutomaticLocationDetection();
                }
            }
            return TRUE;

        case WM_HOTKEY:
            if (wParam == kHotkeyId) ToggleFlyout(bandWindow_ ? bandWindow_->CurrentRect() : FallbackAnchorNearCursor());
            return 0;

        case WM_TIMECHANGE: {
            bool tzChanged = CheckTimeZoneChange();
            RefreshTick(false);
            ScheduleNextTickTimer();
            if (tzChanged && settings_.autoDetectLocation) {
                autoLocRetryCount_ = 0;
                ++autoLocGeneration_;
                StartAutomaticLocationDetection();
            }
            return 0;
        }

        case WM_QUERYENDSESSION:
            return TRUE;

        case WM_ENDSESSION:
            if (wParam != FALSE) PostQuitMessage(0);
            return 0;

        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            if (!shuttingDown_) PostQuitMessage(0);
            return 0;

        case WM_DWMCOLORIZATIONCOLORCHANGED:
        case WM_SETTINGCHANGE:
            InvalidateAccentColorCache();
            if (settingsWindow_) settingsWindow_->RepaintAll();
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
