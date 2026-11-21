#pragma once

#include "../core/models.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace vakit {

class TrayIconHost {
public:
    static constexpr UINT kCallbackMessageId = WM_APP + 42;

    explicit TrayIconHost(HWND ownerHwnd);
    ~TrayIconHost();

    void Update(const PrayerCountdownState& state, const AppSettings& settings, bool force = false);
    void ShowNotification(const std::wstring& title, const std::wstring& body, bool silent);
    void RecreateIfNeeded(const PrayerCountdownState& state, const AppSettings& settings);
    void CleanupTemporaryHiddenIcon();
    void Remove();

private:
    HWND ownerHwnd_ = nullptr;
    HICON currentIcon_ = nullptr;
    bool isAdded_ = false;
    bool isTemporaryHidden_ = false;
    std::wstring lastSignature_;
};

} // namespace vakit
