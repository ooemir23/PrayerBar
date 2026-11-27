#pragma once

#include "models.h"
#include "../platform/win_store.h"
#include <string>

namespace vakit {

class LicenseManager {
public:
    explicit LicenseManager(std::wstring customBaseDir = {}, bool useRegistry = true);

    static DateYMD CurrentLocalDate();

    LicenseStatus GetStatus() const { return GetStatus(CurrentLocalDate()); }
    LicenseStatus GetStatus(DateYMD today) const;

    bool IsProActive() const { return IsProActive(CurrentLocalDate()); }
    bool IsProActive(DateYMD today) const { return GetStatus(today).IsPro(); }

    int64_t FirstLaunchEpochDays() const { return LoadData(CurrentLocalDate()).firstLaunchEpochDays; }
    bool ApplyStoreLicenseSnapshot(const StoreLicenseSnapshot& snap, DateYMD today);

    static bool IsFeatureProOnly(TaskbarDisplayStyle style);
    static bool IsFeatureProOnly(FlyoutDesignStyle design);
    static bool CanPurchaseAnnual(LicenseTier tier);
    static bool CanPurchaseLifetime(LicenseTier tier);

private:
    struct StoredLicenseData {
        int64_t firstLaunchEpochDays = -1;
    };

    StoredLicenseData LoadData(DateYMD today) const;
    bool SaveData(const StoredLicenseData& data) const;
    int64_t SyncFirstLaunchEpochDays(int64_t candidateDay) const;

    std::wstring licensePath_;
    bool useRegistry_ = true;
    LicenseTier storeTier_ = LicenseTier::Free;  // in-memory only, set from Store each launch
    std::wstring storeExpiryIso_;
};

} // namespace vakit
