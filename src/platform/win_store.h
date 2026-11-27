#pragma once

#include "../core/models.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>

namespace vakit {

struct StoreLicenseSnapshot {
    bool storeAvailable = false;
    bool appLicenseActive = false;
    bool appLicenseIsTrial = false;
    bool hasProLifetime = false;
    bool hasProAnnual = false;
    std::wstring annualExpiryIso;
    HRESULT hr = S_OK;
};

enum class StorePurchaseOutcome : int {
    Succeeded = 0,
    AlreadyPurchased = 1,
    NotPurchased = 2,
    NetworkError = 3,
    ServerError = 4,
    NotConfigured = 5,
    StoreUnavailable = 6
};

struct StorePurchaseResponse {
    StorePurchaseOutcome outcome = StorePurchaseOutcome::StoreUnavailable;
    HRESULT hr = S_OK;
    HRESULT extendedError = S_OK;
};

bool ConvertWinRtDateTimeToYmd(int64_t universalTime100ns, DateYMD& outDate);

LicenseTier ClassifyStoreAddOn(
    const std::wstring& skuStoreId,
    const std::wstring& offerToken,
    const std::wstring& configuredAnnualId,
    const std::wstring& configuredLifetimeId);

StoreLicenseSnapshot QueryMicrosoftStoreLicenseSync();

bool BeginMicrosoftStorePurchaseAsync(
    HWND ownerHwnd,
    const std::wstring& addOnStoreId,
    std::function<void(StorePurchaseResponse)> onDone);

enum class StoreUpdateOutcome : int {
    NoUpdate = 0,
    NotPackaged = 1,
    CannotInstallSilently = 2,
    Failed = 3,
    CompletedNeedsRestart = 4
};

StoreUpdateOutcome CheckAndInstallStoreUpdatesSilently();
bool RelaunchPackagedApp(const wchar_t* args);

} // namespace vakit
