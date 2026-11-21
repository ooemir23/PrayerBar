#pragma once

#include "../core/license_manager.h"
#include "../core/models.h"
#include "../core/schedule_service.h"
#include "../core/settings_store.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <functional>
#include <string>
#include <vector>

namespace vakit {

class ImsakiyeWindow {
public:
    static constexpr UINT kMsgMonthSynced = WM_APP + 220;

    ImsakiyeWindow(
        SettingsStore& settingsStore,
        ScheduleService& scheduleService,
        LicenseManager& licenseManager,
        std::function<void()> onRequestUpgrade);
    ~ImsakiyeWindow();

    bool IsOpen() const { return hwnd_ != nullptr && IsWindowVisible(hwnd_); }
    void OpenOrFocus();
    void CloseNow();
    void RefreshIfOpen();

private:
    enum class BtnId { None, PrevMonth, NextMonth, Today, Ramadan, Sync, ExportCsv, ExportHtml };

    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void LoadRows();
    void StartBackgroundSync();
    void ExportCsv();
    void ExportHtml();
    void UpdateScrollBar(int clientW, int clientH);
    void ClampScroll(int clientH);
    void PaintClient(HDC hdc, int width, int height);
    BtnId HitTestToolbar(int x, int y) const;

    SettingsStore& settingsStore_;
    ScheduleService& scheduleService_;
    LicenseManager& licenseManager_;
    std::function<void()> onRequestUpgrade_;

    HWND hwnd_ = nullptr;
    double scale_ = 1.0;
    AppSettings settings_{};

    int viewYear_ = 2026;
    int viewMonth_ = 11;
    bool ramadanMode_ = false;
    DateYMD ramadanStart_{};
    bool syncBusy_ = false;
    std::wstring statusText_;
    std::vector<DailyPrayerTimes> rows_;

    int scrollY_ = 0;
    BtnId hoveredBtn_ = BtnId::None;
    bool trackingLeave_ = false;

    struct ToolbarRects {
        RECT prevRc{};
        RECT nextRc{};
        RECT todayRc{};
        RECT ramadanRc{};
        RECT syncRc{};
        RECT csvRc{};
        RECT htmlRc{};
    } tbRc_{};
};

} // namespace vakit
