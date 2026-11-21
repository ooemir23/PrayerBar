#pragma once

#include "models.h"
#include "../platform/win_store.h"
#include <string>

namespace vakit {

class LicenseManager {
public:
    struct ActivationResult {
        bool ok = false;
        LicenseTier tier = LicenseTier::Free;
        std::wstring errorCode;
    };

    explicit LicenseManager(std::wstring customBaseDir = {}, bool useRegistry = true);

    static DateYMD CurrentLocalDate();

    LicenseStatus GetStatus() const { return GetStatus(CurrentLocalDate()); }
    LicenseStatus GetStatus(DateYMD today) const;

    bool IsProActive() const { return IsProActive(CurrentLocalDate()); }
    bool IsProActive(DateYMD today) const { return GetStatus(today).IsPro(); }

    ActivationResult ActivateKey(const std::wstring& rawKey) {
        return ActivateKey(rawKey, CurrentLocalDate(), true);
    }
    ActivationResult ActivateKey(const std::wstring& rawKey, DateYMD today, bool allowOnlineCheck = true);
    void DeactivateLicense();
    bool ApplyStoreLicenseSnapshot(const StoreLicenseSnapshot& snap, DateYMD today);

    static std::wstring GenerateSignedKeyForTesting(const std::wstring& tierCode, const std::wstring& idPart);
    static bool IsFeatureProOnly(TaskbarDisplayStyle style);
    static bool IsFeatureProOnly(FlyoutDesignStyle design);

private:
    struct StoredLicenseData {
        int64_t firstLaunchEpochDays = -1;
        LicenseTier tier = LicenseTier::Free;
        bool fromMicrosoftStore = false;
        std::wstring storedKey;
        std::wstring expiryIso;
    };

    StoredLicenseData LoadData(DateYMD today) const;
    bool SaveData(const StoredLicenseData& data) const;
    int64_t SyncFirstLaunchEpochDays(int64_t candidateDay) const;

    std::wstring licensePath_;
    bool useRegistry_ = true;
};

} // namespace vakit
