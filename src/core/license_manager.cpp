#include "license_manager.h"
#include "commercial_config.h"
#include "json_mini.h"
#include "settings_store.h"
#include "utf_util.h"
#include "../platform/win_http.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <vector>

namespace vakit {

namespace {

constexpr wchar_t kRegSubKey[] = L"Software\\VakitCubugu";
constexpr wchar_t kRegInstallDayVal[] = L"InstallDay";

constexpr uint32_t kSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

inline uint32_t RotR(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32u - n));
}

std::string Sha256HexUpper(const std::string& input) {
    uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bitLen = static_cast<uint64_t>(msg.size()) * 8ULL;
    msg.push_back(0x80u);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00u);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFFu));
    }

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[64]{};
        for (int i = 0; i < 16; ++i) {
            size_t j = chunk + static_cast<size_t>(i) * 4;
            w[i] = (static_cast<uint32_t>(msg[j]) << 24) |
                   (static_cast<uint32_t>(msg[j + 1]) << 16) |
                   (static_cast<uint32_t>(msg[j + 2]) << 8) |
                   static_cast<uint32_t>(msg[j + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = RotR(w[i - 15], 7) ^ RotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = RotR(w[i - 2], 17) ^ RotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hh + S1 + ch + kSha256K[i] + w[i];
            uint32_t S0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            hh = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(64);
    for (int i = 0; i < 8; ++i) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            out.push_back(kHex[(h[i] >> shift) & 0xFu]);
        }
    }
    return out;
}

std::string NormalizeAsciiUpper(std::wstring_view raw) {
    std::wstring trimmed = TrimWide(raw);
    std::string out;
    out.reserve(trimmed.size());
    for (wchar_t ch : trimmed) {
        if (ch >= L'a' && ch <= L'z') {
            out.push_back(static_cast<char>(ch - L'a' + L'A'));
        } else if (ch >= L' ' && ch <= L'~') {
            out.push_back(static_cast<char>(ch));
        }
    }
    return out;
}

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

std::wstring FormatIsoDate(DateYMD d) {
    wchar_t buf[16];
    swprintf_s(buf, L"%04d-%02d-%02d", d.year, d.month, d.day);
    return std::wstring(buf);
}

std::wstring MaskLicenseKey(const std::wstring& key) {
    if (key.size() <= 8) return key;
    if (key.rfind(L"VKPR-", 0) == 0 && key.size() >= 23) {
        return key.substr(0, 9) + L"-••••-" + key.substr(key.size() - 8);
    }
    return key.substr(0, 4) + L"••••" + key.substr(key.size() - 4);
}

std::string UrlEncode(const std::string& s) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 2);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0xFu]);
            out.push_back(kHex[c & 0xFu]);
        }
    }
    return out;
}

bool ContainsAsciiInsensitive(std::string_view hay, std::string_view needle) {
    if (needle.empty() || hay.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            if (std::tolower(static_cast<unsigned char>(hay[i + j])) !=
                std::tolower(static_cast<unsigned char>(needle[j]))) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

struct OfflineCheckResult {
    bool valid = false;
    bool expired = false;
    LicenseTier tier = LicenseTier::Free;
    std::wstring expiryIso;
};

OfflineCheckResult VerifyOfflineKey(const std::string& upperKey, DateYMD today) {
    OfflineCheckResult res;
    if (upperKey.size() != 27 || upperKey.rfind("VKPR-", 0) != 0) return res;
    if (upperKey[9] != '-' || upperKey[18] != '-') return res;

    std::string tierCode = upperKey.substr(5, 4);
    std::string idPart = upperKey.substr(10, 8);
    std::string sigPart = upperKey.substr(19, 8);

    if (tierCode != "LIFE" && tierCode != "YEAR") return res;
    for (char c : idPart) {
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return res;
    }
    for (char c : sigPart) {
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return res;
    }

    std::string payload = std::string(commercial::kLicenseSignatureSalt) + "|" + tierCode + "|" + idPart;
    std::string expectedSig = Sha256HexUpper(payload).substr(0, 8);
    if (sigPart != expectedSig) return res;

    if (tierCode == "LIFE") {
        res.valid = true;
        res.tier = LicenseTier::ProLifetime;
        return res;
    }

    DateYMD expDate{};
    if (!ParseCompactYmd(idPart, expDate)) return res;
    res.expiryIso = FormatIsoDate(expDate);
    if (expDate < today) {
        res.valid = false;
        res.expired = true;
        return res;
    }
    res.valid = true;
    res.tier = LicenseTier::ProAnnual;
    return res;
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
    int64_t todayEpoch = today.ToEpochDays();
    bool needSave = false;

    std::string raw;
    if (ReadFileUtf8(licensePath_, raw)) {
        if (auto doc = JsonParse(raw); doc && doc->type == JsonValue::Type::Object) {
            d.firstLaunchEpochDays = doc->GetInt("firstLaunchEpochDays", -1);
            int tierInt = doc->GetInt("tier", 0);
            d.tier = static_cast<LicenseTier>(std::clamp(tierInt, 0, 3));
            d.fromMicrosoftStore = doc->GetBool("fromMicrosoftStore", false);
            d.storedKey = Utf8ToWide(doc->GetString("storedKey"));
            d.expiryIso = Utf8ToWide(doc->GetString("expiryIso"));
        }
    }

    if (d.firstLaunchEpochDays <= 0) {
        d.firstLaunchEpochDays = todayEpoch;
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
    root.Set("tier", JsonValue::MakeInt(static_cast<int>(data.tier)));
    root.Set("fromMicrosoftStore", JsonValue::MakeBool(data.fromMicrosoftStore));
    root.Set("storedKey", JsonValue::MakeString(WideToUtf8(data.storedKey)));
    root.Set("expiryIso", JsonValue::MakeString(WideToUtf8(data.expiryIso)));
    return WriteFileAtomicallyUtf8(licensePath_, JsonSerialize(root, true));
}

LicenseStatus LicenseManager::GetStatus(DateYMD today) const {
    StoredLicenseData d = LoadData(today);
    LicenseStatus st;

    if (d.fromMicrosoftStore) {
        if (d.tier == LicenseTier::ProLifetime) {
            st.tier = LicenseTier::ProLifetime;
            st.trialDaysRemaining = 0;
            st.maskedKey = L"Microsoft Store";
            return st;
        }
        if (d.tier == LicenseTier::ProAnnual) {
            bool expired = false;
            DateYMD exp{};
            if (!d.expiryIso.empty() && ParseIsoYmd(d.expiryIso, exp) && exp < today) {
                expired = true;
            }
            if (!expired) {
                st.tier = LicenseTier::ProAnnual;
                st.trialDaysRemaining = 0;
                st.maskedKey = L"Microsoft Store";
                st.expiryIso = d.expiryIso;
                return st;
            }
        }
        st.maskedKey = L"Microsoft Store";
        st.expiryIso = d.expiryIso;
    } else if (!d.storedKey.empty() && (d.tier == LicenseTier::ProAnnual || d.tier == LicenseTier::ProLifetime)) {
        std::string upper = NormalizeAsciiUpper(d.storedKey);
        if (upper.rfind("VKPR-", 0) == 0) {
            OfflineCheckResult chk = VerifyOfflineKey(upper, today);
            if (chk.valid) {
                st.tier = chk.tier;
                st.trialDaysRemaining = 0;
                st.maskedKey = MaskLicenseKey(Utf8ToWide(upper));
                st.expiryIso = chk.expiryIso;
                return st;
            }
        } else {
            bool expired = false;
            DateYMD exp{};
            if (!d.expiryIso.empty() && ParseIsoYmd(d.expiryIso, exp) && exp < today) {
                expired = true;
            }
            if (!expired) {
                st.tier = d.tier;
                st.trialDaysRemaining = 0;
                st.maskedKey = MaskLicenseKey(d.storedKey);
                st.expiryIso = d.expiryIso;
                return st;
            }
        }
        st.maskedKey = MaskLicenseKey(d.storedKey);
        st.expiryIso = d.expiryIso;
    }

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

LicenseManager::ActivationResult LicenseManager::ActivateKey(const std::wstring& rawKey, DateYMD today, bool allowOnlineCheck) {
    ActivationResult res;
    std::string normalized = NormalizeAsciiUpper(rawKey);
    if (normalized.size() < 8) {
        res.errorCode = L"INVALID";
        return res;
    }

    StoredLicenseData data = LoadData(today);

    if (normalized.rfind("VKPR-", 0) == 0) {
        OfflineCheckResult chk = VerifyOfflineKey(normalized, today);
        if (chk.expired) {
            res.errorCode = L"EXPIRED";
            return res;
        }
        if (!chk.valid) {
            res.errorCode = L"INVALID";
            return res;
        }
        data.tier = chk.tier;
        data.storedKey = Utf8ToWide(normalized);
        data.expiryIso = chk.expiryIso;
        SaveData(data);

        res.ok = true;
        res.tier = chk.tier;
        return res;
    }

    if (!allowOnlineCheck) {
        res.errorCode = L"ONLINE_REQUIRED";
        return res;
    }

    std::string rawTrimmedUtf8 = WideToUtf8(TrimWide(rawKey));
    std::optional<std::string> resp;
    bool usingGumroad = (commercial::kGumroadProductPermalink[0] != '\0');
    if (usingGumroad) {
        std::string form = "product_permalink=" + UrlEncode(commercial::kGumroadProductPermalink) +
                           "&license_key=" + UrlEncode(rawTrimmedUtf8) + "&increment_uses_count=true";
        resp = WinHttpPostFormUtf8(commercial::kGumroadHost, commercial::kGumroadVerifyPath, form, 9000);
    } else {
        std::string form = "license_key=" + UrlEncode(rawTrimmedUtf8) + "&instance_name=VakitCubuguWin";
        resp = WinHttpPostFormUtf8(commercial::kLemonSqueezyHost, commercial::kLemonSqueezyActivatePath, form, 9000);
    }

    if (!resp) {
        res.errorCode = L"ONLINE_REQUIRED";
        return res;
    }

    auto doc = JsonParse(*resp);
    if (!doc || doc->type != JsonValue::Type::Object) {
        res.errorCode = L"INVALID";
        return res;
    }

    bool valid = false;
    LicenseTier tier = LicenseTier::ProLifetime;
    std::wstring expiryIso;

    if (usingGumroad) {
        bool success = doc->GetBool("success", false);
        bool refunded = false;
        bool chargebacked = false;
        if (const JsonValue* p = doc->Find("purchase"); p && p->type == JsonValue::Type::Object) {
            refunded = p->GetBool("refunded", false);
            chargebacked = p->GetBool("chargebacked", false);
        }
        valid = success && !refunded && !chargebacked;
    } else {
        bool activated = doc->GetBool("activated", false);
        bool validFlag = doc->GetBool("valid", false);
        std::string status;
        if (const JsonValue* lk = doc->Find("license_key"); lk && lk->type == JsonValue::Type::Object) {
            status = lk->GetString("status");
            std::string exp = lk->GetString("expires_at");
            if (exp.size() >= 10) expiryIso = Utf8ToWide(exp.substr(0, 10));
        }
        valid = activated || validFlag || status == "active";
        if (ContainsAsciiInsensitive(*resp, "annual") || ContainsAsciiInsensitive(*resp, "year")) {
            tier = LicenseTier::ProAnnual;
            if (expiryIso.empty()) {
                expiryIso = FormatIsoDate(today.AddDays(365));
            }
        }
    }

    if (!valid) {
        res.errorCode = L"INVALID";
        return res;
    }

    data.tier = tier;
    data.storedKey = TrimWide(rawKey);
    data.expiryIso = expiryIso;
    SaveData(data);

    res.ok = true;
    res.tier = tier;
    return res;
}

void LicenseManager::DeactivateLicense() {
    StoredLicenseData data = LoadData(CurrentLocalDate());
    data.tier = LicenseTier::Free;
    data.fromMicrosoftStore = false;
    data.storedKey.clear();
    data.expiryIso.clear();
    SaveData(data);
}

bool LicenseManager::ApplyStoreLicenseSnapshot(const StoreLicenseSnapshot& snap, DateYMD today) {
    if (!snap.storeAvailable) {
        return false;
    }

    StoredLicenseData d = LoadData(today);
    LicenseTier oldTier = d.tier;
    bool oldFromStore = d.fromMicrosoftStore;
    std::wstring oldKey = d.storedKey;
    std::wstring oldExp = d.expiryIso;

    if (snap.hasProLifetime) {
        d.tier = LicenseTier::ProLifetime;
        d.fromMicrosoftStore = true;
        d.storedKey = L"MS-STORE-LIFETIME";
        d.expiryIso.clear();
    } else if (snap.hasProAnnual) {
        d.tier = LicenseTier::ProAnnual;
        d.fromMicrosoftStore = true;
        d.storedKey = L"MS-STORE-ANNUAL";
        d.expiryIso = snap.annualExpiryIso;
    } else if (d.fromMicrosoftStore) {
        d.tier = LicenseTier::Free;
        d.fromMicrosoftStore = false;
        d.storedKey.clear();
        d.expiryIso.clear();
    } else {
        return false;
    }

    bool changed = (d.tier != oldTier ||
                    d.fromMicrosoftStore != oldFromStore ||
                    d.storedKey != oldKey ||
                    d.expiryIso != oldExp);
    if (changed) {
        SaveData(d);
    }
    return changed;
}

std::wstring LicenseManager::GenerateSignedKeyForTesting(const std::wstring& tierCode, const std::wstring& idPart) {
    std::string t = NormalizeAsciiUpper(tierCode);
    std::string id = NormalizeAsciiUpper(idPart);
    std::string payload = std::string(commercial::kLicenseSignatureSalt) + "|" + t + "|" + id;
    std::string sig = Sha256HexUpper(payload).substr(0, 8);
    return Utf8ToWide("VKPR-" + t + "-" + id + "-" + sig);
}

bool LicenseManager::IsFeatureProOnly(TaskbarDisplayStyle style) {
    return static_cast<int>(style) >= static_cast<int>(TaskbarDisplayStyle::IconAndCountdown);
}

bool LicenseManager::IsFeatureProOnly(FlyoutDesignStyle design) {
    return design != FlyoutDesignStyle::CompactFluent;
}

} // namespace vakit
