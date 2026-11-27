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
#include <string>
#include <vector>

namespace vakit {

class SettingsWindow {
public:
    static constexpr UINT kMsgLocDetected = WM_APP + 210;
    static constexpr UINT kMsgSyncFinished = WM_APP + 211;
    static constexpr UINT kMsgRebuildUi = WM_APP + 212;
    static constexpr UINT kMsgStorePurchaseDone = WM_APP + 213;
    static constexpr UINT kMsgStoreRestoreDone = WM_APP + 214;
    static constexpr UINT kMsgCitySearchResults = WM_APP + 215;

    SettingsWindow(
        SettingsStore& settingsStore,
        ScheduleService& scheduleService,
        LicenseManager& licenseManager,
        std::function<void()> onSettingsSaved,
        std::function<void(const std::wstring&, const std::wstring&, bool)> onTestNotification,
        std::function<void()> onOpenImsakiye,
        std::function<void(bool)> onRequestUpgrade,
        std::function<bool()> isHotkeyOk = nullptr,
        std::function<void()> onLicenseChanged = nullptr);
    ~SettingsWindow();

    bool IsOpen() const { return hwnd_ != nullptr && IsWindowVisible(hwnd_); }
    void OpenOrFocus(int tab = -1);
    void OpenProTab(bool showLockedHint);
    void CloseNow();
    void RefreshPreviewAndMemory();
    void RefreshLicenseStatus();
    void SyncNotificationControls();
    void ApplyDetectedLocation(const DetectedLocationResult& location);
    void RepaintAll() { if (hwnd_) RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN); }

private:
    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateControls();
    void DestroyAllControls();
    void RebuildUiAfterChange();
    void RebuildFonts();
    void ApplyThemeBrushes();
    void SwitchTab(int tabIdx);
    void UpdateTab0CustomTemplateVisibility();
    void PopulateAllValues();
    void RefreshCityList(const std::wstring& query);
    void StartWorldwideCitySearch(const std::wstring& query);
    void PaintBackgroundAndCards(HDC hdc, int width, int height);
    void PaintTaskbarPreview(HDC hdc, int width, int height);
    void PaintSidebarTab(DRAWITEMSTRUCT* dis, int tabIdx);
    void PaintProTabCanvas(void* g, float s);
    void PaintOwnerPushButton(DRAWITEMSTRUCT* dis);
    void PaintOwnerCheckbox(DRAWITEMSTRUCT* dis);

    void SetFluentCheck(WORD id, HWND h, bool checked);
    bool GetFluentCheck(WORD id) const;
    bool IsControlInCard(WORD id) const;
    bool IsAccentButton(WORD id) const;

    void SaveAndNotify();
    void ApplyNotificationControlsUi();
    void HandleCommand(WORD id, WORD code, HWND ctrl);
    void StartAutoDetectLocation();
    void StartManualSync();
    void StartProStorePurchase(const wchar_t* addOnStoreId);
    void StartProStoreRestore();
    void SetProBusy(bool busy);
    void UpdateProPurchaseButtons();
    void UpdateHotkeyUi();
    void BeginHotkeyCapture();
    static LRESULT CALLBACK HotkeyBtnProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR ref);
    static LRESULT CALLBACK HoverBtnProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR ref);

    SettingsStore& settingsStore_;
    ScheduleService& scheduleService_;
    LicenseManager& licenseManager_;
    std::function<void()> onSettingsSaved_;
    std::function<void(const std::wstring&, const std::wstring&, bool)> onTestNotification_;
    std::function<void()> onOpenImsakiye_;
    std::function<void(bool)> onRequestUpgrade_;
    std::function<bool()> isHotkeyOk_;
    std::function<void()> onLicenseChanged_;
    bool capturingHotkey_ = false;

    HWND hwnd_ = nullptr;
    HWND hoveredBtn_ = nullptr;
    double scale_ = 1.0;
    int activeTab_ = 0;
    bool isLoading_ = false;
    bool isDark_ = true;
    unsigned int locationRequestGeneration_ = 0;
    AppSettings settings_{};
    std::vector<CityLocation> visibleCities_;

    HFONT uiFont_ = nullptr;
    HFONT boldFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    HFONT titleFont_ = nullptr;
    HFONT priceFont_ = nullptr;
    HBRUSH bgBrush_ = nullptr;
    HBRUSH cardBrush_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    HBRUSH sidebarBrush_ = nullptr;

    std::vector<HWND> allCreatedControls_;
    std::vector<HWND> tabControls_[6];
    std::set<HWND> cardStaticLabels_;
    std::set<HWND> mutedStaticLabels_;
    std::set<WORD> rightToggleIds_;
    std::map<WORD, bool> checkStates_;
    std::map<WORD, bool> checkInCard_;

    HWND tabBtns_[6]{};
    HWND previewBox_ = nullptr;
    HWND footerInfo_ = nullptr;
    HWND okBtn_ = nullptr;

    // Tab 0 (Görünüm)
    HWND styleCombo_ = nullptr;
    HWND presetBtns_[6]{};
    HWND customTemplateLabel_ = nullptr;
    HWND customEdit_ = nullptr;
    HWND tokenBtns_[7]{};
    HWND flyoutCombo_ = nullptr;
    HWND flyoutScaleSlider_ = nullptr;
    HWND flyoutScaleLabel_ = nullptr;
    HWND dockCombo_ = nullptr;
    HWND offsetSlider_ = nullptr;
    HWND offsetLabel_ = nullptr;
    HWND showPillChk_ = nullptr;
    HWND opaqueChk_ = nullptr;
    HWND showTrayChk_ = nullptr;
    HWND showBarChk_ = nullptr;
    HWND showSecChk_ = nullptr;
    HWND accentChk_ = nullptr;
    HWND taskbarFrameChk_ = nullptr;
    HWND taskbarLogoChk_ = nullptr;
    HWND trayCombo_ = nullptr;
    HWND trayMin99Chk_ = nullptr;
    HWND fontCombo_ = nullptr;
    HWND urgentCombo_ = nullptr;
    HWND themeCombo_ = nullptr;

    // Tab 1 (Konum & Vakitler)
    HWND detectLocBtn_ = nullptr;
    HWND autoLocStatus_ = nullptr;
    HWND autoDetectLocChk_ = nullptr;
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
    HWND notifyAllChk_ = nullptr;
    HWND alertPreCombo_[6]{};
    HWND alertSoundChk_[6]{};
    HWND entrySoundCombo_ = nullptr;
    HWND preSoundCombo_ = nullptr;
    HWND fajrSoundCombo_ = nullptr;
    HWND customSoundEdit_ = nullptr;
    HWND browseSoundBtn_ = nullptr;
    HWND volumeSlider_ = nullptr;
    HWND volumeLabel_ = nullptr;
    HWND quietFullscreenChk_ = nullptr;
    HWND kerahatChk_ = nullptr;
    HWND ramadanChk_ = nullptr;
    HWND fridayChk_ = nullptr;

    // Tab 3 (Özellikler)
    HWND holidayChk_ = nullptr;
    HWND extraTimesChk_ = nullptr;
    HWND travelChk_ = nullptr;
    HWND trackingChk_ = nullptr;
    HWND toastChk_ = nullptr;
    HWND hotkeyChk_ = nullptr;
    HWND hotkeyBtn_ = nullptr;
    HWND hotkeyDesc_ = nullptr;

    // Tab 4 (Dil & Sistem)
    HWND langCombo_ = nullptr;
    HWND customNameEdits_[6]{};
    HWND customShortEdits_[6]{};
    HWND startupChk_ = nullptr;
    HWND autoMemChk_ = nullptr;
    HWND telemetryLabel_ = nullptr;
    HWND sysStatusLabel_ = nullptr;
    HWND licenseLabel_ = nullptr;

    // Tab 5 (Pro & Lisans)
    HWND proRestoreBtn_ = nullptr;
    HWND proManageBtn_ = nullptr;
    HWND proBuyAnnualBtn_ = nullptr;
    HWND proBuyLifetimeBtn_ = nullptr;
    bool proBusy_ = false;
    bool proLockedHint_ = false;
    std::wstring proFeedbackText_;
    bool proFeedbackIsError_ = false;
};

} // namespace vakit
