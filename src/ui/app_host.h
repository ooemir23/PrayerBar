#pragma once

#include "../core/license_manager.h"
#include "../core/models.h"
#include "../core/schedule_service.h"
#include "../core/settings_store.h"
#include "flyout_window.h"
#include "imsakiye_window.h"
#include "pro_window.h"
#include "settings_window.h"
#include "taskbar_band.h"
#include "tray_icon.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <memory>
#include <set>
#include <string>

namespace vakit {

class AppHost {
public:
    static constexpr wchar_t kHostWindowClassName[] = L"PrayerBarHostClass";
    static constexpr UINT kMsgShowFlyout = WM_APP + 101;
    static constexpr UINT kMsgScheduleRefreshed = WM_APP + 103;
    static constexpr UINT kMsgStoreLicenseSynced = WM_APP + 104;
    static constexpr UINT kMsgShowSettings = WM_APP + 105;  // wParam = tab index
    static constexpr UINT kMsgAutoLocationDetected = WM_APP + 106;

    AppHost(SettingsStore& settingsStore, ScheduleService& scheduleService);
    ~AppHost();

    int Run();

private:
    static LRESULT CALLBACK HostWndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HostWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void RefreshTick(bool checkNotifications);
    void ScheduleNextTickTimer();
    void EvaluateNotifications(const LocalDateTime& now, const PrayerCountdownState& state);
    void ToggleFlyout(RECT anchorRect);
    void TriggerBackgroundRefresh();
    void StartAutomaticLocationDetection();
    void ShowUserNotification(const std::wstring& title, const std::wstring& body, bool silent, bool hasAudio = false);
    void ShowActionBalloon(const std::wstring& title, const std::wstring& body, int action);
    void CheckTimeZoneChange();
    void ApplyHotkey();
    void OnSettingsChanged();

    SettingsStore& settingsStore_;
    ScheduleService& scheduleService_;
    LicenseManager licenseManager_;
    AppSettings settings_{};

    HWND hostHwnd_ = nullptr;
    UINT taskbarCreatedMsg_ = 0;
    std::unique_ptr<TaskbarBandWindow> bandWindow_;
    std::unique_ptr<TrayIconHost> trayHost_;
    std::unique_ptr<FlyoutWindow> flyoutWindow_;
    std::unique_ptr<ImsakiyeWindow> imsakiyeWindow_;
    std::unique_ptr<ProWindow> proWindow_;
    std::unique_ptr<SettingsWindow> settingsWindow_;
    std::set<std::wstring> sentNotifications_;
    bool hotkeyOk_ = true;          // false when RegisterHotKey failed (combination taken)
    int pendingBalloonAction_ = 0;  // what a click on the current tray balloon does (see kBalloon*)
    bool balloonStopsAudio_ = false;
};

} // namespace vakit
