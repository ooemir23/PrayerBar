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
#include <map>
#include <set>
#include <vector>

namespace vakit {

class SettingsWindow {
public:
    static constexpr UINT kMsgLocDetected = WM_APP + 210;
    static constexpr UINT kMsgSyncFinished = WM_APP + 211;
    static constexpr UINT kMsgRebuildUi = WM_APP + 212;

    SettingsWindow(
        SettingsStore& settingsStore,
        ScheduleService& scheduleService,
        LicenseManager& licenseManager,
        std::function<void()> onSettingsSaved,
        std::function<void(const std::wstring&, const std::wstring&, bool)> onTestNotification,
        std::function<void()> onOpenImsakiye,
        std::function<void(bool)> onRequestUpgrade);
    ~SettingsWindow();

    bool IsOpen() const { return hwnd_ != nullptr && IsWindowVisible(hwnd_); }
    void OpenOrFocus();
    void CloseNow();
    void RefreshPreviewAndMemory();
    void RefreshLicenseStatus();

private:
    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateControls();
    void DestroyAllControls();
    void RebuildUiAfterChange();
    void RebuildFonts();
    void ApplyThemeBrushes();
    void SwitchTab(int tabIdx);
    void PopulateAllValues();
    void RefreshCityList(const std::wstring& query);
    void PaintBackgroundAndCards(HDC hdc, int width, int height);
    void PaintTaskbarPreview(HDC hdc, int width, int height);
    void PaintTabHeader(HDC hdc, int tabIdx, const RECT& rc);
    void PaintOwnerPushButton(DRAWITEMSTRUCT* dis);
    void PaintOwnerCheckbox(DRAWITEMSTRUCT* dis);

    void SetFluentCheck(WORD id, HWND h, bool checked);
    bool GetFluentCheck(WORD id) const;
    bool IsControlInCard(WORD id) const;
    bool IsAccentButton(WORD id) const;

    void SaveAndNotify();
    void HandleCommand(WORD id, WORD code, HWND ctrl);
    void StartAutoDetectLocation();
    void StartManualSync();

    SettingsStore& settingsStore_;
    ScheduleService& scheduleService_;
    LicenseManager& licenseManager_;
    std::function<void()> onSettingsSaved_;
    std::function<void(const std::wstring&, const std::wstring&, bool)> onTestNotification_;
    std::function<void()> onOpenImsakiye_;
    std::function<void(bool)> onRequestUpgrade_;

    HWND hwnd_ = nullptr;
    double scale_ = 1.0;
    int activeTab_ = 0;
    bool isLoading_ = false;
    bool isDark_ = true;
    AppSettings settings_{};
    std::vector<CityLocation> visibleCities_;

    HFONT uiFont_ = nullptr;
    HFONT boldFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    HBRUSH bgBrush_ = nullptr;
    HBRUSH cardBrush_ = nullptr;
    HBRUSH editBrush_ = nullptr;

    std::vector<HWND> allCreatedControls_;
    std::vector<HWND> tabControls_[4];
    std::set<HWND> cardStaticLabels_;
    std::map<WORD, bool> checkStates_;
    std::map<WORD, bool> checkInCard_;

    HWND tabBtns_[4]{};
    HWND previewBox_ = nullptr;
    HWND footerInfo_ = nullptr;
    HWND okBtn_ = nullptr;

    // Tab 0 (Görünüm)
    HWND styleCombo_ = nullptr;
    HWND customEdit_ = nullptr;
    HWND flyoutCombo_ = nullptr;
    HWND dockCombo_ = nullptr;
    HWND offsetSlider_ = nullptr;
    HWND offsetLabel_ = nullptr;
    HWND showPillChk_ = nullptr;
    HWND opaqueChk_ = nullptr;
    HWND showTrayChk_ = nullptr;
    HWND showBarChk_ = nullptr;
    HWND showSecChk_ = nullptr;
    HWND accentChk_ = nullptr;
    HWND trayCombo_ = nullptr;
    HWND fontCombo_ = nullptr;
    HWND urgentCombo_ = nullptr;
    HWND themeCombo_ = nullptr;

    // Tab 1 (Konum & Vakitler)
    HWND detectLocBtn_ = nullptr;
    HWND autoLocStatus_ = nullptr;
    HWND autoMethodChk_ = nullptr;
    HWND citySearchEdit_ = nullptr;
    HWND cityList_ = nullptr;
    HWND customCoordChk_ = nullptr;
    HWND latEdit_ = nullptr;
    HWND lonEdit_ = nullptr;
    HWND applyCoordBtn_ = nullptr;
    HWND methodCombo_ = nullptr;
    HWND asrCombo_ = nullptr;
    HWND onlineSyncChk_ = nullptr;
    HWND syncBtn_ = nullptr;
    HWND openImsakiyeBtn_ = nullptr;
    HWND syncStatus_ = nullptr;
    HWND offsetEdits_[6]{};

    // Tab 2 (Bildirim & Ses)
    HWND alertAtTimeChk_[6]{};
    HWND alertPreCombo_[6]{};
    HWND alertSoundChk_[6]{};
    HWND entrySoundCombo_ = nullptr;
    HWND preSoundCombo_ = nullptr;
    HWND customSoundEdit_ = nullptr;
    HWND browseSoundBtn_ = nullptr;
    HWND volumeSlider_ = nullptr;
    HWND volumeLabel_ = nullptr;
    HWND quietFullscreenChk_ = nullptr;
    HWND kerahatChk_ = nullptr;
    HWND ramadanChk_ = nullptr;

    // Tab 3 (Dil & Sistem)
    HWND langCombo_ = nullptr;
    HWND customNameEdits_[6]{};
    HWND customShortEdits_[6]{};
    HWND startupChk_ = nullptr;
    HWND autoMemChk_ = nullptr;
    HWND telemetryLabel_ = nullptr;
    HWND sysStatusLabel_ = nullptr;
    HWND licenseLabel_ = nullptr;
};

} // namespace vakit
