#pragma once

#include "../core/license_manager.h"
#include "../core/models.h"
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

namespace vakit {

class ProWindow {
public:
    static constexpr UINT kMsgStorePurchaseDone = WM_APP + 231;
    static constexpr UINT kMsgStoreRestoreDone = WM_APP + 232;

    ProWindow(
        SettingsStore& settingsStore,
        LicenseManager& licenseManager,
        std::function<void()> onLicenseChanged);
    ~ProWindow();

    bool IsOpen() const { return hwnd_ != nullptr && IsWindowVisible(hwnd_); }
    void OpenOrFocus(bool showLockedHint = false);
    void CloseNow();
    void RefreshThemeOrLanguage();

private:
    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateControls();
    void RebuildFonts();
    void ApplyThemeBrushes();
    void RefreshStatusTexts();
    void PaintCanvas(HDC hdc, int width, int height);
    void PaintOwnerPushButton(DRAWITEMSTRUCT* dis);
    void SetBusy(bool busy);
    void UpdatePurchaseButtons();
    void StartStorePurchase(const wchar_t* addOnStoreId);
    void StartStoreRestore(bool afterPurchase);

    SettingsStore& settingsStore_;
    LicenseManager& licenseManager_;
    std::function<void()> onLicenseChanged_;

    HWND hwnd_ = nullptr;
    double scale_ = 1.0;
    bool isDark_ = true;
    bool showLockedHint_ = false;
    bool busy_ = false;
    AppSettings settings_{};
    LicenseStatus status_{};

    HFONT uiFont_ = nullptr;
    HFONT boldFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    HBRUSH bgBrush_ = nullptr;
    HBRUSH cardBrush_ = nullptr;

    HWND buyAnnualBtn_ = nullptr;
    HWND buyLifetimeBtn_ = nullptr;
    HWND restoreBtn_ = nullptr;
    HWND manageSubBtn_ = nullptr;
    HWND rateStoreBtn_ = nullptr;
    HWND closeBtn_ = nullptr;
    std::wstring feedbackText_;
    bool feedbackIsError_ = false;
};

} // namespace vakit
