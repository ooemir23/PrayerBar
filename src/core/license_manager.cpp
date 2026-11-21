#include "license_manager.h"
#include "commercial_config.h"
#include "json_mini.h"
#include "settings_store.h"
#include "utf_util.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>

namespace vakit {

namespace {

constexpr wchar_t kRegSubKey[] = L"Software\\VakitCubugu";
constexpr wchar_t kRegInstallDayVal[] = L"InstallDay";

bool ParseCompactYmd(const std::string& s, DateYMD& out) {
    if (s.size() != 8) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    int y = std::stoi(s.substr(0, 4));
    int m = std::stoi(s.substr(4, 2));
    int d = std::stoi(s.substr(6, 2));
    if (y < 2024 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    out = {y, m, d};
    return true;
}

bool ParseIsoYmd(const std::wstring& s, DateYMD& out) {
    if (s.size() < 10 || s[4] != L'-' || s[7] != L'-') return false;
    std::string compact;
    compact.reserve(8);
    for (size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u}) {
        if (s[i] < L'0' || s[i] > L'9') return false;
        compact.push_back(static_cast<char>(s[i]));
    }
    return ParseCompactYmd(compact, out);
}

} // namespace

LicenseManager::LicenseManager(std::wstring customBaseDir, bool useRegistry)
    : licensePath_((customBaseDir.empty() ? GetDefaultLocalAppDataDir() : std::move(customBaseDir)) + L"\\license.json"),
      useRegistry_(useRegistry) {
    LoadData(CurrentLocalDate());
}

DateYMD LicenseManager::CurrentLocalDate() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return {st.wYear, st.wMonth, st.wDay};
}

int64_t LicenseManager::SyncFirstLaunchEpochDays(int64_t candidateDay) const {
    if (!useRegistry_) return candidateDay;

    DWORD existing = 0;
    DWORD sz = sizeof(existing);
    if (RegGetValueW(HKEY_CURRENT_USER, kRegSubKey, kRegInstallDayVal, RRF_RT_REG_DWORD, nullptr, &existing, &sz) == ERROR_SUCCESS &&
        existing > 0) {
        int64_t regDay = static_cast<int64_t>(existing);
        if (candidateDay < regDay) {
            HKEY hk = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegSubKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
                DWORD val = static_cast<DWORD>(std::max<int64_t>(1, candidateDay));
                RegSetValueExW(hk, kRegInstallDayVal, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
                RegCloseKey(hk);
            }
            return candidateDay;
        }
        return regDay;
    }

    HKEY hk = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegSubKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &hk, nullptr) == ERROR_SUCCESS) {
        DWORD val = static_cast<DWORD>(std::max<int64_t>(1, candidateDay));
        RegSetValueExW(hk, kRegInstallDayVal, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        RegCloseKey(hk);
    }
    return candidateDay;
}

LicenseManager::StoredLicenseData LicenseManager::LoadData(DateYMD today) const {
    StoredLicenseData d;
    bool needSave = false;

    std::string raw;
    if (ReadFileUtf8(licensePath_, raw)) {
        if (auto doc = JsonParse(raw); doc && doc->type == JsonValue::Type::Object) {
            d.firstLaunchEpochDays = doc->GetInt("firstLaunchEpochDays", -1);
        }
    }

    if (d.firstLaunchEpochDays <= 0) {
        d.firstLaunchEpochDays = today.ToEpochDays();
        needSave = true;
    }

    int64_t synced = SyncFirstLaunchEpochDays(d.firstLaunchEpochDays);
    if (synced != d.firstLaunchEpochDays) {
        d.firstLaunchEpochDays = synced;
        needSave = true;
    }

    if (needSave) {
        SaveData(d);
    }
    return d;
}

bool LicenseManager::SaveData(const StoredLicenseData& data) const {
    JsonValue root = JsonValue::MakeObject();
    root.Set("firstLaunchEpochDays", JsonValue::MakeInt(static_cast<int>(std::clamp<int64_t>(data.firstLaunchEpochDays, 0, 10000000LL))));
    return WriteFileAtomicallyUtf8(licensePath_, JsonSerialize(root, true));
}

LicenseStatus LicenseManager::GetStatus(DateYMD today) const {
    LicenseStatus st;

    // Pro is only ever granted from this session's Store query; nothing on disk can grant it.
    if (storeTier_ == LicenseTier::ProLifetime) {
        st.tier = LicenseTier::ProLifetime;
        st.trialDaysRemaining = 0;
        st.maskedKey = L"Microsoft Store";
        return st;
    }
    if (storeTier_ == LicenseTier::ProAnnual) {
        DateYMD exp{};
        if (storeExpiryIso_.empty() || !ParseIsoYmd(storeExpiryIso_, exp) || !(exp < today)) {
            st.tier = LicenseTier::ProAnnual;
            st.trialDaysRemaining = 0;
            st.maskedKey = L"Microsoft Store";
            st.expiryIso = storeExpiryIso_;
            return st;
        }
    }

    StoredLicenseData d = LoadData(today);
    int64_t elapsed = std::max<int64_t>(0, today.ToEpochDays() - d.firstLaunchEpochDays);
    if (elapsed < commercial::kTrialDays) {
        st.tier = LicenseTier::Trial;
        st.trialDaysRemaining = commercial::kTrialDays - static_cast<int>(elapsed);
    } else {
        st.tier = LicenseTier::Free;
        st.trialDaysRemaining = 0;
    }
    return st;
}

bool LicenseManager::ApplyStoreLicenseSnapshot(const StoreLicenseSnapshot& snap, DateYMD /*today*/) {
    if (!snap.storeAvailable) {
        return false;
    }

    LicenseTier newTier = LicenseTier::Free;
    std::wstring newExp;
    if (snap.hasProLifetime) {
        newTier = LicenseTier::ProLifetime;
    } else if (snap.hasProAnnual) {
        newTier = LicenseTier::ProAnnual;
        newExp = snap.annualExpiryIso;
    }

    bool changed = (newTier != storeTier_ || newExp != storeExpiryIso_);
    storeTier_ = newTier;
    storeExpiryIso_ = std::move(newExp);
    return changed;
}

bool LicenseManager::IsFeatureProOnly(TaskbarDisplayStyle style) {
    return static_cast<int>(style) >= static_cast<int>(TaskbarDisplayStyle::IconAndCountdown);
}

bool LicenseManager::IsFeatureProOnly(FlyoutDesignStyle design) {
    return design != FlyoutDesignStyle::CompactFluent;
}

} // namespace vakit
