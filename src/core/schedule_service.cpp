#include "schedule_service.h"
#include "city_catalog.h"
#include "json_mini.h"
#include "prayer_calc.h"
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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <initguid.h>
#include <locationapi.h>

namespace vakit {

namespace {

bool ParseTimeHHMM(std::string_view raw, TimeHM& out) {
    size_t sp = raw.find(' ');
    std::string_view hhmm = (sp != std::string_view::npos) ? raw.substr(0, sp) : raw;
    if (hhmm.size() < 5 || hhmm[2] != ':') return false;
    int h = (hhmm[0] - '0') * 10 + (hhmm[1] - '0');
    int m = (hhmm[3] - '0') * 10 + (hhmm[4] - '0');
    if (h < 0 || h > 23 || m < 0 || m > 59) return false;
    out = {h, m};
    return true;
}

bool ParseDateDDMMYYYY(std::string_view raw, DateYMD& out) {
    if (raw.size() < 10 || raw[2] != '-' || raw[5] != '-') return false;
    int d = (raw[0] - '0') * 10 + (raw[1] - '0');
    int m = (raw[3] - '0') * 10 + (raw[4] - '0');
    int y = (raw[6] - '0') * 1000 + (raw[7] - '0') * 100 + (raw[8] - '0') * 10 + (raw[9] - '0');
    if (y < 1900 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    out = {y, m, d};
    return true;
}

bool ParseDateYYYYMMDD(std::string_view raw, DateYMD& out) {
    if (raw.size() < 10 || raw[4] != '-' || raw[7] != '-') return false;
    int y = (raw[0] - '0') * 1000 + (raw[1] - '0') * 100 + (raw[2] - '0') * 10 + (raw[3] - '0');
    int m = (raw[5] - '0') * 10 + (raw[6] - '0');
    int d = (raw[8] - '0') * 10 + (raw[9] - '0');
    if (y < 1900 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    out = {y, m, d};
    return true;
}

DailyPrayerTimes ApplyOffsets(const DailyPrayerTimes& raw, const PrayerOffsets& offsets) {
    if (offsets.IsAllZero()) return raw;
    DailyPrayerTimes copy = raw;
    copy.imsak = raw.imsak.AddMinutes(offsets.imsak);
    copy.gunes = raw.gunes.AddMinutes(offsets.gunes);
    copy.ogle = raw.ogle.AddMinutes(offsets.ogle);
    copy.ikindi = raw.ikindi.AddMinutes(offsets.ikindi);
    copy.aksam = raw.aksam.AddMinutes(offsets.aksam);
    copy.yatsi = raw.yatsi.AddMinutes(offsets.yatsi);
    return copy;
}

std::wstring MapIanaStaticFallback(std::string_view iana) {
    if (iana == "Europe/Istanbul" || iana == "Asia/Istanbul") return L"Turkey Standard Time";
    if (iana == "Europe/Berlin" || iana == "Europe/Vienna" || iana == "Europe/Amsterdam" ||
        iana == "Europe/Rome" || iana == "Europe/Stockholm" || iana == "Europe/Copenhagen" ||
        iana == "Europe/Oslo" || iana == "Europe/Zurich" || iana == "Europe/Madrid") return L"W. Europe Standard Time";
    if (iana == "Europe/Paris" || iana == "Europe/Brussels" || iana == "Europe/Luxembourg") return L"Romance Standard Time";
    if (iana == "Europe/London" || iana == "Europe/Lisbon" || iana == "Europe/Dublin") return L"GMT Standard Time";
    if (iana == "Europe/Sarajevo" || iana == "Europe/Prague" || iana == "Europe/Warsaw" ||
        iana == "Europe/Budapest" || iana == "Europe/Belgrade" || iana == "Europe/Zagreb" ||
        iana == "Europe/Tirane" || iana == "Europe/Skopje") return L"Central European Standard Time";
    if (iana == "Europe/Athens" || iana == "Europe/Helsinki" || iana == "Europe/Bucharest" ||
        iana == "Europe/Sofia" || iana == "Europe/Kyiv" || iana == "Europe/Kiev" ||
        iana == "Europe/Riga" || iana == "Europe/Tallinn" || iana == "Europe/Vilnius" ||
        iana == "Asia/Nicosia" || iana == "Europe/Nicosia") return L"FLE Standard Time";
    if (iana == "Europe/Chisinau") return L"E. Europe Standard Time";
    if (iana == "Europe/Moscow" || iana == "Europe/Minsk") return L"Russian Standard Time";
    if (iana == "Asia/Baku") return L"Azerbaijan Standard Time";
    if (iana == "Asia/Tbilisi") return L"Georgian Standard Time";
    if (iana == "Asia/Yerevan") return L"Caucasus Standard Time";
    if (iana == "Asia/Riyadh" || iana == "Asia/Qatar" || iana == "Asia/Kuwait" ||
        iana == "Asia/Baghdad" || iana == "Asia/Bahrain" || iana == "Asia/Aden") return L"Arab Standard Time";
    if (iana == "Asia/Dubai" || iana == "Asia/Muscat") return L"Arabian Standard Time";
    if (iana == "Asia/Amman") return L"Jordan Standard Time";
    if (iana == "Asia/Tehran") return L"Iran Standard Time";
    if (iana == "Asia/Karachi") return L"Pakistan Standard Time";
    if (iana == "Asia/Kabul") return L"Afghanistan Standard Time";
    if (iana == "Asia/Kolkata" || iana == "Asia/Calcutta") return L"India Standard Time";
    if (iana == "Asia/Colombo") return L"Sri Lanka Standard Time";
    if (iana == "Asia/Dhaka") return L"Bangladesh Standard Time";
    if (iana == "Asia/Kathmandu") return L"Nepal Standard Time";
    if (iana == "Asia/Yangon") return L"Myanmar Standard Time";
    if (iana == "Asia/Kuala_Lumpur" || iana == "Asia/Kuching" || iana == "Asia/Singapore" ||
        iana == "Asia/Manila" || iana == "Asia/Makassar") return L"Singapore Standard Time";
    if (iana == "Asia/Jakarta" || iana == "Asia/Bangkok" || iana == "Asia/Ho_Chi_Minh") return L"SE Asia Standard Time";
    if (iana == "Asia/Jayapura") return L"Tokyo Standard Time";
    if (iana == "Asia/Tashkent" || iana == "Asia/Ashgabat") return L"West Asia Standard Time";
    if (iana == "Asia/Almaty" || iana == "Asia/Bishkek") return L"Central Asia Standard Time";
    if (iana == "Asia/Dushanbe") return L"West Asia Standard Time";
    if (iana == "Asia/Yekaterinburg") return L"Ekaterinburg Standard Time";
    if (iana == "Asia/Krasnoyarsk") return L"North Asia Standard Time";
    if (iana == "Asia/Vladivostok") return L"Vladivostok Standard Time";
    if (iana == "Asia/Shanghai" || iana == "Asia/Hong_Kong" || iana == "Asia/Urumqi") return L"China Standard Time";
    if (iana == "Asia/Jerusalem") return L"Israel Standard Time";
    if (iana == "Asia/Gaza" || iana == "Asia/Hebron") return L"West Bank Standard Time";
    if (iana == "Asia/Beirut") return L"Middle East Standard Time";
    if (iana == "Asia/Damascus") return L"Syria Standard Time";
    if (iana == "Asia/Seoul") return L"Korea Standard Time";
    if (iana == "Asia/Taipei") return L"Taipei Standard Time";
    if (iana == "Africa/Cairo") return L"Egypt Standard Time";
    if (iana == "Africa/Casablanca") return L"Morocco Standard Time";
    if (iana == "Africa/Tunis" || iana == "Africa/Algiers" || iana == "Africa/Lagos") return L"W. Central Africa Standard Time";
    if (iana == "Africa/Nairobi" || iana == "Africa/Addis_Ababa") return L"E. Africa Standard Time";
    if (iana == "Africa/Johannesburg") return L"South Africa Standard Time";
    if (iana == "Africa/Accra" || iana == "Africa/Abidjan") return L"Greenwich Standard Time";
    if (iana == "Africa/Tripoli") return L"Libya Standard Time";
    if (iana == "Africa/Khartoum") return L"Sudan Standard Time";
    if (iana == "Africa/Windhoek") return L"Namibia Standard Time";
    if (iana == "America/New_York" || iana == "America/Toronto" || iana == "America/Detroit" ||
        iana == "America/Montreal") return L"Eastern Standard Time";
    if (iana == "America/Chicago" || iana == "America/Winnipeg" || iana == "America/Mexico_City" ||
        iana == "America/Monterrey") return L"Central Standard Time";
    if (iana == "America/Regina") return L"Canada Central Standard Time";
    if (iana == "America/Denver" || iana == "America/Edmonton") return L"Mountain Standard Time";
    if (iana == "America/Phoenix") return L"US Mountain Standard Time";
    if (iana == "America/Los_Angeles" || iana == "America/Vancouver" || iana == "America/Tijuana") return L"Pacific Standard Time";
    if (iana == "America/Anchorage") return L"Alaskan Standard Time";
    if (iana == "Pacific/Honolulu") return L"Hawaiian Standard Time";
    if (iana == "America/Halifax") return L"Atlantic Standard Time";
    if (iana == "America/St_Johns") return L"Newfoundland Standard Time";
    if (iana == "America/Sao_Paulo") return L"E. South America Standard Time";
    if (iana == "America/Argentina/Buenos_Aires") return L"Argentina Standard Time";
    if (iana == "America/Bogota" || iana == "America/Lima" || iana == "America/Guayaquil") return L"SA Pacific Standard Time";
    if (iana == "America/La_Paz" || iana == "America/Puerto_Rico") return L"SA Western Standard Time";
    if (iana == "America/Caracas") return L"Venezuela Standard Time";
    if (iana == "America/Montevideo") return L"Montevideo Standard Time";
    if (iana == "America/Santiago") return L"Pacific SA Standard Time";
    if (iana == "Asia/Tokyo") return L"Tokyo Standard Time";
    if (iana == "Australia/Sydney" || iana == "Australia/Melbourne" ||
        iana == "Australia/Canberra" || iana == "Australia/Hobart") return L"AUS Eastern Standard Time";
    if (iana == "Australia/Adelaide") return L"Cen. Australia Standard Time";
    if (iana == "Australia/Darwin") return L"AUS Central Standard Time";
    if (iana == "Australia/Brisbane") return L"E. Australia Standard Time";
    if (iana == "Australia/Perth") return L"W. Australia Standard Time";
    if (iana == "Pacific/Auckland") return L"New Zealand Standard Time";
    if (iana == "Pacific/Fiji") return L"Fiji Standard Time";
    if (iana == "Pacific/Apia") return L"Samoa Standard Time";
    if (iana == "Pacific/Tongatapu") return L"Tonga Standard Time";
    return L"";
}

std::wstring GetSystemTimeZoneId() {
    DYNAMIC_TIME_ZONE_INFORMATION tz{};
    if (GetDynamicTimeZoneInformation(&tz) == TIME_ZONE_ID_INVALID) return L"";
    return tz.TimeZoneKeyName;
}

std::wstring GetSystemRegionCountryCode() {
    using GetGeoNameFn = int(WINAPI*)(LPWSTR, int);
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (!k32) return L"";
    auto fn = reinterpret_cast<GetGeoNameFn>(reinterpret_cast<void*>(GetProcAddress(k32, "GetUserDefaultGeoName")));
    if (!fn) return L"";
    wchar_t buf[16]{};
    int len = fn(buf, 16);
    if (len >= 2) {
        std::wstring code(buf);
        if (code.size() == 2 &&
            ((code[0] >= L'A' && code[0] <= L'Z') || (code[0] >= L'a' && code[0] <= L'z')) &&
            ((code[1] >= L'A' && code[1] <= L'Z') || (code[1] >= L'a' && code[1] <= L'z'))) {
            return code;
        }
    }
    return L"";
}

bool IsValidCoordinates(double lat, double lon) {
    if (!std::isfinite(lat) || !std::isfinite(lon)) return false;
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) return false;
    if (std::abs(lat) < 0.0001 && std::abs(lon) < 0.0001) return false;
    return true;
}

std::string CleanGeoField(std::string_view raw) {
    size_t start = 0;
    size_t end = raw.size();
    auto isWs = [](char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; };
    while (start < end && isWs(raw[start])) ++start;
    while (end > start && isWs(raw[end - 1])) --end;
    std::string_view trimmed = raw.substr(start, end - start);
    if (trimmed.empty()) return {};

    auto eqCi = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            char ca = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
            if (ca != b[i]) return false;
        }
        return true;
    };
    if (eqCi(trimmed, "nil") || eqCi(trimmed, "null") || eqCi(trimmed, "none") ||
        eqCi(trimmed, "unknown") || trimmed == "-") {
        return {};
    }
    return std::string(trimmed);
}

struct WindowsLocationFix {
    double latitude = 0.0;
    double longitude = 0.0;
    std::wstring civicCity;
    std::wstring civicCountry;
};

struct GeoRawComUnknown {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
    };
    const Vtbl* lpVtbl;
};

struct GeoAbiAsyncInfo {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Id)(void* self, uint32_t* id);
        HRESULT (STDMETHODCALLTYPE* get_Status)(void* self, int32_t* status);
        HRESULT (STDMETHODCALLTYPE* get_ErrorCode)(void* self, HRESULT* errorCode);
        HRESULT (STDMETHODCALLTYPE* Cancel)(void* self);
        HRESULT (STDMETHODCALLTYPE* Close)(void* self);
    };
    const Vtbl* lpVtbl;
};

struct GeoAbiAsyncOperation {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* put_Completed)(void* self, void* handler);
        HRESULT (STDMETHODCALLTYPE* get_Completed)(void* self, void** handler);
        HRESULT (STDMETHODCALLTYPE* GetResults)(void* self, void** results);
    };
    const Vtbl* lpVtbl;
};

struct GeoAbiGeolocator {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_DesiredAccuracy)(void* self, int32_t* value);
        HRESULT (STDMETHODCALLTYPE* put_DesiredAccuracy)(void* self, int32_t value);
        HRESULT (STDMETHODCALLTYPE* get_MovementThreshold)(void* self, double* value);
        HRESULT (STDMETHODCALLTYPE* put_MovementThreshold)(void* self, double value);
        HRESULT (STDMETHODCALLTYPE* get_ReportInterval)(void* self, uint32_t* value);
        HRESULT (STDMETHODCALLTYPE* put_ReportInterval)(void* self, uint32_t value);
        HRESULT (STDMETHODCALLTYPE* get_LocationStatus)(void* self, int32_t* value);
        HRESULT (STDMETHODCALLTYPE* GetGeopositionAsync)(void* self, GeoAbiAsyncOperation** value);
        HRESULT (STDMETHODCALLTYPE* GetGeopositionAsyncWithAgeAndTimeout)(void* self, int64_t maximumAge, int64_t timeout, GeoAbiAsyncOperation** value);
    };
    const Vtbl* lpVtbl;
};

struct GeoAbiGeoposition {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Coordinate)(void* self, void** value);
        HRESULT (STDMETHODCALLTYPE* get_CivicAddress)(void* self, void** value);
    };
    const Vtbl* lpVtbl;
};

struct GeoAbiGeocoordinate {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Latitude)(void* self, double* value);
        HRESULT (STDMETHODCALLTYPE* get_Longitude)(void* self, double* value);
    };
    const Vtbl* lpVtbl;
};

struct GeoAbiGeocoordinateWithPoint {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Point)(void* self, void** value);
    };
    const Vtbl* lpVtbl;
};

struct GeoBasicGeoposition {
    double latitude;
    double longitude;
    double altitude;
};

struct GeoAbiGeopoint {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Position)(void* self, GeoBasicGeoposition* value);
    };
    const Vtbl* lpVtbl;
};

struct GeoAbiCivicAddress {
    struct Vtbl {
        HRESULT (STDMETHODCALLTYPE* QueryInterface)(void* self, REFIID riid, void** ppv);
        ULONG (STDMETHODCALLTYPE* AddRef)(void* self);
        ULONG (STDMETHODCALLTYPE* Release)(void* self);
        void* getIids;
        void* getRuntimeClassName;
        void* getTrustLevel;
        HRESULT (STDMETHODCALLTYPE* get_Country)(void* self, void** value);
        HRESULT (STDMETHODCALLTYPE* get_State)(void* self, void** value);
        HRESULT (STDMETHODCALLTYPE* get_City)(void* self, void** value);
    };
    const Vtbl* lpVtbl;
};

template <typename T>
void GeoSafeRelease(T*& ptr) {
    if (ptr) {
        reinterpret_cast<GeoRawComUnknown*>(ptr)->lpVtbl->Release(ptr);
        ptr = nullptr;
    }
}

std::optional<WindowsLocationFix> TryGetWinRtGeolocation() {
    HMODULE combase = GetModuleHandleW(L"combase.dll");
    if (!combase) combase = LoadLibraryW(L"combase.dll");
    if (!combase) return std::nullopt;

    auto roInit = reinterpret_cast<HRESULT (WINAPI*)(int)>(GetProcAddress(combase, "RoInitialize"));
    auto roUninit = reinterpret_cast<void (WINAPI*)()>(GetProcAddress(combase, "RoUninitialize"));
    auto roActivate = reinterpret_cast<HRESULT (WINAPI*)(void*, void**)>(GetProcAddress(combase, "RoActivateInstance"));
    auto createStr = reinterpret_cast<HRESULT (WINAPI*)(const wchar_t*, UINT32, void**)>(GetProcAddress(combase, "WindowsCreateString"));
    auto deleteStr = reinterpret_cast<HRESULT (WINAPI*)(void*)>(GetProcAddress(combase, "WindowsDeleteString"));
    auto getRawBuf = reinterpret_cast<const wchar_t* (WINAPI*)(void*, UINT32*)>(GetProcAddress(combase, "WindowsGetStringRawBuffer"));
    if (!roInit || !roUninit || !roActivate || !createStr || !deleteStr || !getRawBuf) {
        return std::nullopt;
    }

    HRESULT roHr = roInit(1 /* RO_INIT_MULTITHREADED */);
    bool shouldUninit = (roHr == S_OK || roHr == S_FALSE);
    if (FAILED(roHr) && roHr != RPC_E_CHANGED_MODE) {
        return std::nullopt;
    }

    static constexpr IID kIidAsyncInfo = {0x00000036u, 0x0000u, 0x0000u, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
    static constexpr IID kIidGeolocator = {0xA9C3BF62u, 0x4524u, 0x4989u, {0x8A, 0xA9, 0xDE, 0x01, 0x9D, 0x2E, 0x55, 0x1F}};
    static constexpr IID kIidGeoposition = {0xC18D0454u, 0x7D41u, 0x4FF7u, {0xA9, 0x57, 0x9D, 0xFF, 0xB4, 0xEF, 0x7F, 0x5B}};
    static constexpr IID kIidGeocoordinate = {0xEE21A3AAu, 0x976Au, 0x4C70u, {0x80, 0x3D, 0x08, 0x3E, 0xA5, 0x5B, 0xCB, 0xC4}};
    static constexpr IID kIidGeocoordinateWithPoint = {0xFEEA0525u, 0xD22Cu, 0x4D46u, {0xB5, 0x27, 0x0B, 0x96, 0x06, 0x6F, 0xC7, 0xDB}};
    static constexpr IID kIidGeopoint = {0x6BFA00EBu, 0xE56Eu, 0x49BBu, {0x9C, 0xAF, 0xCB, 0xAA, 0x78, 0xA8, 0xBC, 0xEF}};
    static constexpr IID kIidCivicAddress = {0xA8567A1Au, 0x64F4u, 0x4D48u, {0xBC, 0xEA, 0xF6, 0xB0, 0x08, 0xEC, 0xA3, 0x4C}};

    std::optional<WindowsLocationFix> result;
    void* hsClass = nullptr;
    void* rawInst = nullptr;
    GeoAbiGeolocator* geolocator = nullptr;
    GeoAbiAsyncOperation* op = nullptr;
    GeoAbiAsyncInfo* asyncInfo = nullptr;
    void* rawPos = nullptr;
    GeoAbiGeoposition* geoposition = nullptr;
    void* rawCoord = nullptr;
    GeoAbiGeocoordinateWithPoint* coordWithPoint = nullptr;
    void* rawPoint = nullptr;
    GeoAbiGeopoint* geopoint = nullptr;
    GeoAbiGeocoordinate* coord = nullptr;
    void* rawCivic = nullptr;
    GeoAbiCivicAddress* civic = nullptr;

    auto consumeHStr = [&](void* hs) -> std::wstring {
        if (!hs) return {};
        UINT32 len = 0;
        const wchar_t* buf = getRawBuf(hs, &len);
        std::wstring s = (buf && len > 0) ? std::wstring(buf, len) : std::wstring();
        deleteStr(hs);
        return s;
    };

    do {
        constexpr wchar_t kClass[] = L"Windows.Devices.Geolocation.Geolocator";
        if (FAILED(createStr(kClass, static_cast<UINT32>(wcslen(kClass)), &hsClass)) || !hsClass) break;
        if (FAILED(roActivate(hsClass, &rawInst)) || !rawInst) break;

        auto* rawUnk = static_cast<GeoRawComUnknown*>(rawInst);
        if (FAILED(rawUnk->lpVtbl->QueryInterface(rawUnk, kIidGeolocator, reinterpret_cast<void**>(&geolocator))) || !geolocator) {
            break;
        }

        int32_t locStatus = 0;
        if (SUCCEEDED(geolocator->lpVtbl->get_LocationStatus(geolocator, &locStatus))) {
            // 2 = Disabled, 5 = NotAvailable
            if (locStatus == 2 || locStatus == 5) break;
        }

        constexpr int64_t kMaxAge100Ns = 15LL * 60LL * 10000000LL;
        constexpr int64_t kTimeout100Ns = 5LL * 10000000LL;
        if (FAILED(geolocator->lpVtbl->GetGeopositionAsyncWithAgeAndTimeout(geolocator, kMaxAge100Ns, kTimeout100Ns, &op)) || !op) {
            break;
        }

        if (FAILED(op->lpVtbl->QueryInterface(op, kIidAsyncInfo, reinterpret_cast<void**>(&asyncInfo))) || !asyncInfo) {
            break;
        }

        int32_t asyncStatus = 0;
        for (int waitedMs = 0; waitedMs < 6000; waitedMs += 25) {
            if (FAILED(asyncInfo->lpVtbl->get_Status(asyncInfo, &asyncStatus)) || asyncStatus != 0) {
                break;
            }
            Sleep(25);
        }

        if (asyncStatus == 0) {
            asyncInfo->lpVtbl->Cancel(asyncInfo);
            break;
        }
        if (asyncStatus != 1) break;

        if (FAILED(op->lpVtbl->GetResults(op, &rawPos)) || !rawPos) break;
        auto* posUnk = static_cast<GeoRawComUnknown*>(rawPos);
        if (FAILED(posUnk->lpVtbl->QueryInterface(posUnk, kIidGeoposition, reinterpret_cast<void**>(&geoposition))) || !geoposition) {
            break;
        }

        if (FAILED(geoposition->lpVtbl->get_Coordinate(geoposition, &rawCoord)) || !rawCoord) break;
        auto* coordUnk = static_cast<GeoRawComUnknown*>(rawCoord);

        double lat = 999.0;
        double lon = 999.0;
        if (SUCCEEDED(coordUnk->lpVtbl->QueryInterface(coordUnk, kIidGeocoordinateWithPoint, reinterpret_cast<void**>(&coordWithPoint))) && coordWithPoint) {
            if (SUCCEEDED(coordWithPoint->lpVtbl->get_Point(coordWithPoint, &rawPoint)) && rawPoint) {
                auto* ptUnk = static_cast<GeoRawComUnknown*>(rawPoint);
                if (SUCCEEDED(ptUnk->lpVtbl->QueryInterface(ptUnk, kIidGeopoint, reinterpret_cast<void**>(&geopoint))) && geopoint) {
                    GeoBasicGeoposition bg{};
                    if (SUCCEEDED(geopoint->lpVtbl->get_Position(geopoint, &bg))) {
                        lat = bg.latitude;
                        lon = bg.longitude;
                    }
                }
            }
        }
        if (!IsValidCoordinates(lat, lon)) {
            if (SUCCEEDED(coordUnk->lpVtbl->QueryInterface(coordUnk, kIidGeocoordinate, reinterpret_cast<void**>(&coord))) && coord) {
                coord->lpVtbl->get_Latitude(coord, &lat);
                coord->lpVtbl->get_Longitude(coord, &lon);
            }
        }

        if (IsValidCoordinates(lat, lon)) {
            WindowsLocationFix fix;
            fix.latitude = lat;
            fix.longitude = lon;
            if (SUCCEEDED(geoposition->lpVtbl->get_CivicAddress(geoposition, &rawCivic)) && rawCivic) {
                auto* civUnk = static_cast<GeoRawComUnknown*>(rawCivic);
                if (SUCCEEDED(civUnk->lpVtbl->QueryInterface(civUnk, kIidCivicAddress, reinterpret_cast<void**>(&civic))) && civic) {
                    void* hsCity = nullptr;
                    void* hsCountry = nullptr;
                    if (SUCCEEDED(civic->lpVtbl->get_City(civic, &hsCity))) {
                        fix.civicCity = TrimWide(consumeHStr(hsCity));
                    }
                    if (SUCCEEDED(civic->lpVtbl->get_Country(civic, &hsCountry))) {
                        fix.civicCountry = TrimWide(consumeHStr(hsCountry));
                    }
                }
            }
            result = std::move(fix);
        }
    } while (false);

    GeoSafeRelease(civic);
    GeoSafeRelease(rawCivic);
    GeoSafeRelease(coord);
    GeoSafeRelease(geopoint);
    GeoSafeRelease(rawPoint);
    GeoSafeRelease(coordWithPoint);
    GeoSafeRelease(rawCoord);
    GeoSafeRelease(geoposition);
    GeoSafeRelease(rawPos);
    GeoSafeRelease(asyncInfo);
    GeoSafeRelease(op);
    GeoSafeRelease(geolocator);
    GeoSafeRelease(rawInst);
    if (hsClass) deleteStr(hsClass);
    if (shouldUninit) roUninit();

    return result;
}

std::optional<WindowsLocationFix> GetWindowsLocationCoordinates() {
    if (auto winRtFix = TryGetWinRtGeolocation()) {
        return winRtFix;
    }

    HRESULT initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool shouldUninit = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return std::nullopt;
    }

    std::optional<WindowsLocationFix> coordinates;
    ILocation* location = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_Location, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ILocation, reinterpret_cast<void**>(&location))) && location) {
        ILocationReport* baseReport = nullptr;
        if (SUCCEEDED(location->GetReport(IID_ILatLongReport, &baseReport)) && baseReport) {
            auto* report = static_cast<ILatLongReport*>(baseReport);
            DOUBLE latitude = 0.0;
            DOUBLE longitude = 0.0;
            if (SUCCEEDED(report->GetLatitude(&latitude)) && SUCCEEDED(report->GetLongitude(&longitude)) &&
                IsValidCoordinates(latitude, longitude)) {
                WindowsLocationFix fix;
                fix.latitude = latitude;
                fix.longitude = longitude;
                coordinates = std::move(fix);
            }
            baseReport->Release();
        }
        location->Release();
    }

    if (shouldUninit) CoUninitialize();
    return coordinates;
}

std::string EncodeQueryParameter(std::string_view value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            result.push_back(static_cast<char>(c));
        } else {
            result.push_back('%');
            result.push_back(kHex[c >> 4]);
            result.push_back(kHex[c & 0x0F]);
        }
    }
    return result;
}

PrayerCountdownState BuildState(
    const LocalDateTime& nowLocal,
    PrayerKind currentPrayer,
    const LocalDateTime& prevDt,
    PrayerKind nextPrayer,
    const LocalDateTime& nextDt,
    const DailyPrayerTimes& today,
    TimeHM tomorrowImsak,
    const AppSettings& settings) {

    int64_t nowSec = nowLocal.ToEpochSeconds();
    int64_t prevSec = prevDt.ToEpochSeconds();
    int64_t nextSec = nextDt.ToEpochSeconds();

    int64_t remSec = std::max<int64_t>(0, nextSec - nowSec);
    int64_t totalSec = nextSec - prevSec;
    double progress = totalSec > 1 ? std::clamp(static_cast<double>(nowSec - prevSec) / static_cast<double>(totalSec), 0.0, 1.0) : 0.0;
    bool isUrgent = settings.highlightUrgentMinutes > 0 &&
                    (static_cast<double>(remSec) / 60.0) <= static_cast<double>(settings.highlightUrgentMinutes);

    return PrayerCountdownState{
        nowLocal,
        currentPrayer,
        nextPrayer,
        nextDt,
        prevDt,
        remSec,
        progress,
        isUrgent,
        today,
        tomorrowImsak,
        ComputeKerahatStatus(nowLocal, today)
    };
}

int DaysInGregorianMonth(int year, int month) {
    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
        case 4: case 6: case 9: case 11: return 30;
        case 2: {
            bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
            return leap ? 29 : 28;
        }
        default: return 30;
    }
}

} // namespace

ScheduleService::ScheduleService(std::wstring customCacheDir)
    : cacheDir_(customCacheDir.empty() ? (GetDefaultLocalAppDataDir() + L"\\cache") : std::move(customCacheDir)) {}

LocalDateTime ScheduleService::GetCurrentLocalDateTime() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return {{st.wYear, st.wMonth, st.wDay}, st.wHour, st.wMinute, st.wSecond};
}

std::wstring ScheduleService::BuildMonthKey(int year, int month, const AppSettings& settings) {
    wchar_t buf[256];
    bool coordinateBased = settings.useCustomCoordinates || settings.cityId == L"custom" ||
                           settings.cityId.rfind(L"world_", 0) == 0;
    if (coordinateBased) {
        wchar_t coordBuf[96];
        swprintf_s(coordBuf, L"%.4f_%.4f", settings.latitude, settings.longitude);
        for (wchar_t* p = coordBuf; *p; ++p) {
            if (*p == L'.' || *p == L',') *p = L'p';
        }
        uint32_t tzHash = 2166136261u;
        for (wchar_t ch : settings.timeZoneId) {
            tzHash = (tzHash ^ static_cast<uint32_t>(ch)) * 16777619u;
        }
        swprintf_s(buf, L"%ls_%08X_%d_%d_%04d%02d", coordBuf, tzHash,
                   static_cast<int>(settings.calculationMethod),
                   static_cast<int>(settings.asrMethod), year, month);
    } else {
        swprintf_s(buf, L"%ls_%d_%d_%04d%02d", settings.cityId.c_str(),
                   static_cast<int>(settings.calculationMethod),
                   static_cast<int>(settings.asrMethod), year, month);
    }
    return std::wstring(buf);
}

PrayerCountdownState ScheduleService::ComputeState(const LocalDateTime& nowLocal, const AppSettings& settings) {
    DateYMD todayDate = nowLocal.date;
    DateYMD yesterdayDate = todayDate.AddDays(-1);
    DateYMD tomorrowDate = todayDate.AddDays(1);

    DailyPrayerTimes yesterday = GetOrCalculateDay(yesterdayDate, settings, false);
    DailyPrayerTimes today = GetOrCalculateDay(todayDate, settings, settings.useOnlineSync);
    DailyPrayerTimes tomorrow = GetOrCalculateDay(tomorrowDate, settings, settings.useOnlineSync && tomorrowDate.month != todayDate.month);

    LocalDateTime todayImsakDt = LocalDateTime::FromDateTime(todayDate, today.imsak);
    if (nowLocal < todayImsakDt) {
        LocalDateTime prevYatsiDt = LocalDateTime::FromDateTime(yesterdayDate, yesterday.yatsi);
        return BuildState(nowLocal, PrayerKind::Yatsi, prevYatsiDt, PrayerKind::Imsak, todayImsakDt, today, tomorrow.imsak, settings);
    }

    auto entries = today.ToOrderedEntries();
    for (size_t i = 0; i + 1 < entries.size(); ++i) {
        LocalDateTime nextDt = LocalDateTime::FromDateTime(todayDate, entries[i + 1].second);
        if (nowLocal < nextDt) {
            LocalDateTime prevDt = LocalDateTime::FromDateTime(todayDate, entries[i].second);
            return BuildState(nowLocal, entries[i].first, prevDt, entries[i + 1].first, nextDt, today, tomorrow.imsak, settings);
        }
    }

    LocalDateTime todayYatsiDt = LocalDateTime::FromDateTime(todayDate, today.yatsi);
    LocalDateTime tomorrowImsakDt = LocalDateTime::FromDateTime(tomorrowDate, tomorrow.imsak);
    return BuildState(nowLocal, PrayerKind::Yatsi, todayYatsiDt, PrayerKind::Imsak, tomorrowImsakDt, today, tomorrow.imsak, settings);
}

DailyPrayerTimes ScheduleService::GetDaySchedule(DateYMD date, const AppSettings& settings) {
    return GetOrCalculateDay(date, settings, false);
}

std::vector<DailyPrayerTimes> ScheduleService::GetMonthSchedule(int year, int month, const AppSettings& settings, bool triggerSync) {
    int m = std::clamp(month, 1, 12);
    int nDays = DaysInGregorianMonth(year, m);
    std::wstring monthKey = BuildMonthKey(year, m, settings);

    std::map<DateYMD, DailyPrayerTimes> monthMap;
    bool cached = TryLoadMonth(monthKey, monthMap);
    if (!cached && triggerSync && settings.useOnlineSync) {
        TriggerBackgroundFetch(monthKey, year, m, settings);
    }

    std::vector<DailyPrayerTimes> days;
    days.reserve(static_cast<size_t>(nDays));
    for (int day = 1; day <= nDays; ++day) {
        DateYMD d{year, m, day};
        if (cached) {
            auto it = monthMap.find(d);
            if (it != monthMap.end()) {
                days.push_back(ApplyOffsets(it->second, settings.offsets));
                continue;
            }
        }
        days.push_back(CalculatePrayerTimes(
            d,
            settings.latitude,
            settings.longitude,
            settings.timeZoneId,
            settings.calculationMethod,
            settings.offsets,
            settings.asrMethod));
    }
    return days;
}

void ScheduleService::TrimMemoryCache(DateYMD keepDate) {
    std::lock_guard<std::mutex> lk(mutex_);
    missingDiskMonths_.clear();
    computedDays_.clear();
    if (memoryMonths_.size() <= 4) return;

    auto makeYmSuffix = [](int y, int m) -> std::wstring {
        wchar_t buf[16];
        swprintf_s(buf, L"_%04d%02d", y, m);
        return std::wstring(buf);
    };

    int prevY = keepDate.month == 1 ? keepDate.year - 1 : keepDate.year;
    int prevM = keepDate.month == 1 ? 12 : keepDate.month - 1;
    int nextY = keepDate.month == 12 ? keepDate.year + 1 : keepDate.year;
    int nextM = keepDate.month == 12 ? 1 : keepDate.month + 1;

    std::wstring curSuf = makeYmSuffix(keepDate.year, keepDate.month);
    std::wstring prevSuf = makeYmSuffix(prevY, prevM);
    std::wstring nextSuf = makeYmSuffix(nextY, nextM);

    auto endsWith = [](const std::wstring& s, const std::wstring& suf) {
        return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
    };

    for (auto it = memoryMonths_.begin(); it != memoryMonths_.end();) {
        if (!endsWith(it->first, curSuf) && !endsWith(it->first, prevSuf) && !endsWith(it->first, nextSuf)) {
            it = memoryMonths_.erase(it);
        } else {
            ++it;
        }
    }
}

namespace {

bool OffsetsEqual(const PrayerOffsets& a, const PrayerOffsets& b) {
    return a.imsak == b.imsak && a.gunes == b.gunes && a.ogle == b.ogle &&
           a.ikindi == b.ikindi && a.aksam == b.aksam && a.yatsi == b.yatsi;
}

bool ParseMonthFromDisk(const std::wstring& cacheDir, const std::wstring& monthKey, std::map<DateYMD, DailyPrayerTimes>& loaded) {
    std::wstring path = cacheDir + L"\\schedule_" + monthKey + L".json";
    std::string raw;
    if (!ReadFileUtf8(path, raw)) return false;

    auto parsed = JsonParse(raw);
    if (!parsed || parsed->type != JsonValue::Type::Object) return false;
    const JsonValue* daysArr = parsed->Find("days");
    if (!daysArr || daysArr->type != JsonValue::Type::Array || daysArr->arrVal.empty()) return false;

    for (const auto& item : daysArr->arrVal) {
        if (item.type != JsonValue::Type::Object) continue;
        DailyPrayerTimes d;
        if (!ParseDateYYYYMMDD(item.GetString("date"), d.date)) continue;
        if (!ParseTimeHHMM(item.GetString("imsak"), d.imsak)) continue;
        if (!ParseTimeHHMM(item.GetString("gunes"), d.gunes)) continue;
        if (!ParseTimeHHMM(item.GetString("ogle"), d.ogle)) continue;
        if (!ParseTimeHHMM(item.GetString("ikindi"), d.ikindi)) continue;
        if (!ParseTimeHHMM(item.GetString("aksam"), d.aksam)) continue;
        if (!ParseTimeHHMM(item.GetString("yatsi"), d.yatsi)) continue;
        d.hijriDateText = Utf8ToWide(item.GetString("hijriDateText"));
        d.source = Utf8ToWide(item.GetString("source", "Aladhan"));
        loaded[d.date] = std::move(d);
    }
    return !loaded.empty();
}

} // namespace

DailyPrayerTimes ScheduleService::GetOrCalculateDay(DateYMD date, const AppSettings& settings, bool triggerSync) {
    std::wstring monthKey = BuildMonthKey(date.year, date.month, settings);
    DailyPrayerTimes cachedDay{};
    if (TryGetCachedDay(monthKey, date, cachedDay)) {
        return ApplyOffsets(cachedDay, settings.offsets);
    }

    if (triggerSync) {
        TriggerBackgroundFetch(monthKey, date.year, date.month, settings);
    }

    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& entry : computedDays_) {
            if (entry.date == date && entry.monthKey == monthKey && OffsetsEqual(entry.offsets, settings.offsets)) {
                return entry.times;
            }
        }
    }

    DailyPrayerTimes calculated = CalculatePrayerTimes(
        date,
        settings.latitude,
        settings.longitude,
        settings.timeZoneId,
        settings.calculationMethod,
        settings.offsets,
        settings.asrMethod);

    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (computedDays_.size() >= 16) {
            computedDays_.erase(computedDays_.begin());
        }
        computedDays_.push_back({std::move(monthKey), date, settings.offsets, calculated});
    }
    return calculated;
}

bool ScheduleService::TryGetCachedDay(const std::wstring& monthKey, DateYMD date, DailyPrayerTimes& outDay) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = memoryMonths_.find(monthKey);
        if (it != memoryMonths_.end()) {
            auto dayIt = it->second.find(date);
            if (dayIt != it->second.end()) {
                outDay = dayIt->second;
                return true;
            }
            return false;
        }
        if (missingDiskMonths_.count(monthKey) > 0) {
            return false;
        }
    }

    std::map<DateYMD, DailyPrayerTimes> loaded;
    if (!ParseMonthFromDisk(cacheDir_, monthKey, loaded)) {
        std::lock_guard<std::mutex> lk(mutex_);
        missingDiskMonths_.insert(monthKey);
        return false;
    }

    bool found = false;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto& stored = memoryMonths_[monthKey];
        if (stored.empty()) {
            stored = std::move(loaded);
        }
        auto dayIt = stored.find(date);
        if (dayIt != stored.end()) {
            outDay = dayIt->second;
            found = true;
        }
    }
    return found;
}

bool ScheduleService::TryLoadMonth(const std::wstring& monthKey, std::map<DateYMD, DailyPrayerTimes>& outMap) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = memoryMonths_.find(monthKey);
        if (it != memoryMonths_.end()) {
            outMap = it->second;
            return true;
        }
        if (missingDiskMonths_.count(monthKey) > 0) {
            return false;
        }
    }

    std::map<DateYMD, DailyPrayerTimes> loaded;
    if (!ParseMonthFromDisk(cacheDir_, monthKey, loaded)) {
        std::lock_guard<std::mutex> lk(mutex_);
        missingDiskMonths_.insert(monthKey);
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(mutex_);
        memoryMonths_[monthKey] = loaded;
    }
    outMap = std::move(loaded);
    return true;
}

void ScheduleService::SaveMonthToCache(const std::wstring& monthKey, const std::vector<DailyPrayerTimes>& days) {
    JsonValue root = JsonValue::MakeObject();
    root.Set("cacheKey", JsonValue::MakeString(WideToUtf8(monthKey)));
    JsonValue arr = JsonValue::MakeArray();

    std::map<DateYMD, DailyPrayerTimes> mapCopy;
    for (const auto& d : days) {
        char dateBuf[16];
        snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d", d.date.year, d.date.month, d.date.day);
        JsonValue item = JsonValue::MakeObject();
        item.Set("date", JsonValue::MakeString(dateBuf));
        item.Set("imsak", JsonValue::MakeString(WideToUtf8(d.imsak.FormatHHMM())));
        item.Set("gunes", JsonValue::MakeString(WideToUtf8(d.gunes.FormatHHMM())));
        item.Set("ogle", JsonValue::MakeString(WideToUtf8(d.ogle.FormatHHMM())));
        item.Set("ikindi", JsonValue::MakeString(WideToUtf8(d.ikindi.FormatHHMM())));
        item.Set("aksam", JsonValue::MakeString(WideToUtf8(d.aksam.FormatHHMM())));
        item.Set("yatsi", JsonValue::MakeString(WideToUtf8(d.yatsi.FormatHHMM())));
        item.Set("hijriDateText", JsonValue::MakeString(WideToUtf8(d.hijriDateText)));
        item.Set("source", JsonValue::MakeString(WideToUtf8(d.source)));
        arr.arrVal.push_back(std::move(item));
        mapCopy[d.date] = d;
    }
    root.Set("days", std::move(arr));

    EnsureDirectoryRecursive(cacheDir_);
    std::wstring path = cacheDir_ + L"\\schedule_" + monthKey + L".json";
    WriteFileAtomicallyUtf8(path, JsonSerialize(root, false));

    std::lock_guard<std::mutex> lk(mutex_);
    missingDiskMonths_.erase(monthKey);
    computedDays_.erase(
        std::remove_if(computedDays_.begin(), computedDays_.end(),
                       [&](const ComputedDayEntry& e) { return e.monthKey == monthKey; }),
        computedDays_.end());
    memoryMonths_[monthKey] = std::move(mapCopy);
}

bool ScheduleService::ForceRefreshMonth(DateYMD date, const AppSettings& settings) {
    std::wstring monthKey = BuildMonthKey(date.year, date.month, settings);
    auto fetched = FetchAladhanMonth(date.year, date.month, settings.latitude, settings.longitude,
                                     settings.calculationMethod, settings.asrMethod);
    if (!fetched || fetched->empty()) return false;
    SaveMonthToCache(monthKey, *fetched);
    if (onRefreshed_) onRefreshed_();
    return true;
}

void ScheduleService::TriggerBackgroundFetch(const std::wstring& monthKey, int year, int month, const AppSettings& settings) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!inFlightKeys_.insert(monthKey).second) return;
    }

    double lat = settings.latitude;
    double lon = settings.longitude;
    CalculationMethod method = settings.calculationMethod;
    AsrJuristicMethod asrMethod = settings.asrMethod;

    std::thread([this, monthKey, year, month, lat, lon, method, asrMethod]() {
        auto fetched = FetchAladhanMonth(year, month, lat, lon, method, asrMethod);
        if (fetched && !fetched->empty()) {
            SaveMonthToCache(monthKey, *fetched);
            if (onRefreshed_) onRefreshed_();
        }
        std::lock_guard<std::mutex> lk(mutex_);
        inFlightKeys_.erase(monthKey);
    }).detach();
}

std::optional<std::vector<DailyPrayerTimes>> ScheduleService::FetchAladhanMonth(
    int year, int month, double lat, double lon, CalculationMethod method, AsrJuristicMethod asrMethod) {

    wchar_t pathBuf[256];
    swprintf_s(pathBuf, L"/v1/calendar/%d/%d?latitude=%.4f&longitude=%.4f&method=%d&school=%d",
               year, month, lat, lon, static_cast<int>(method), static_cast<int>(asrMethod));

    auto body = WinHttpGetUtf8(L"api.aladhan.com", pathBuf, true, 8000);
    if (!body) return std::nullopt;

    auto parsed = JsonParse(*body);
    if (!parsed || parsed->type != JsonValue::Type::Object) return std::nullopt;
    const JsonValue* data = parsed->Find("data");
    if (!data || data->type != JsonValue::Type::Array) return std::nullopt;

    std::wstring srcLabel = (method == CalculationMethod::Diyanet)
        ? L"Diyanet / Aladhan"
        : (std::wstring(L"Aladhan · ") + GetMethodFriendlyName(method));

    std::vector<DailyPrayerTimes> result;
    result.reserve(data->arrVal.size());

    for (const auto& item : data->arrVal) {
        const JsonValue* timings = item.Find("timings");
        const JsonValue* dateObj = item.Find("date");
        if (!timings || !dateObj) continue;
        const JsonValue* greg = dateObj->Find("gregorian");
        if (!greg) continue;

        DailyPrayerTimes d;
        if (!ParseDateDDMMYYYY(greg->GetString("date"), d.date)) continue;
        if (!ParseTimeHHMM(timings->GetString("Fajr"), d.imsak)) continue;
        if (!ParseTimeHHMM(timings->GetString("Sunrise"), d.gunes)) continue;
        if (!ParseTimeHHMM(timings->GetString("Dhuhr"), d.ogle)) continue;
        if (!ParseTimeHHMM(timings->GetString("Asr"), d.ikindi)) continue;
        if (!ParseTimeHHMM(timings->GetString("Maghrib"), d.aksam)) continue;
        if (!ParseTimeHHMM(timings->GetString("Isha"), d.yatsi)) continue;

        std::wstring hijriText;
        if (const JsonValue* hijri = dateObj->Find("hijri")) {
            std::string hDay = hijri->GetString("day");
            std::string hYear = hijri->GetString("year");
            int mNum = 0;
            if (const JsonValue* mObj = hijri->Find("month")) {
                mNum = mObj->GetInt("number", 0);
            }
            const wchar_t* mTr = TranslateHijriMonthName(mNum);
            if (!hDay.empty() && !hYear.empty() && mTr[0]) {
                while (hDay.size() > 1 && hDay[0] == '0') hDay.erase(hDay.begin());
                hijriText = Utf8ToWide(hDay) + L" " + mTr + L" " + Utf8ToWide(hYear);
            }
        }
        d.hijriDateText = hijriText.empty() ? FormatTurkishHijriDate(d.date) : std::move(hijriText);
        d.source = srcLabel;
        result.push_back(std::move(d));
    }

    if (result.empty()) return std::nullopt;
    return result;
}

std::wstring ScheduleService::MapIanaToWindowsTimeZone(std::string_view iana) {
    if (iana.empty()) return L"";

    using UcalGetWinTzFn = int32_t(__cdecl*)(const wchar_t*, int32_t, wchar_t*, int32_t, int32_t*);
    static UcalGetWinTzFn icuFn = []() -> UcalGetWinTzFn {
        HMODULE icu = GetModuleHandleW(L"icu.dll");
        if (!icu) icu = LoadLibraryW(L"icu.dll");
        if (!icu) return nullptr;
        return reinterpret_cast<UcalGetWinTzFn>(reinterpret_cast<void*>(GetProcAddress(icu, "ucal_getWindowsTimeZoneID")));
    }();
    if (icuFn) {
        std::wstring ianaW = Utf8ToWide(iana);
        if (!ianaW.empty()) {
            wchar_t winBuf[128]{};
            int32_t status = 0;
            int32_t len = icuFn(ianaW.c_str(), static_cast<int32_t>(ianaW.size()), winBuf, 128, &status);
            if (status <= 0 && len > 0 && len < 128) {
                return std::wstring(winBuf, static_cast<size_t>(len));
            }
        }
    }

    return MapIanaStaticFallback(iana);
}

namespace {

DetectedLocationResult BuildDetectedLocation(
    double lat,
    double lon,
    std::string_view cityRaw,
    std::string_view countryRaw,
    std::string_view ccRaw,
    std::string_view ianaTzRaw,
    const wchar_t* sourceLabel) {

    std::string city = CleanGeoField(cityRaw);
    std::string country = CleanGeoField(countryRaw);
    std::string cc = CleanGeoField(ccRaw);
    std::string ianaTz = CleanGeoField(ianaTzRaw);

    auto nearest = FindNearestCity(lat, lon, 30.0);
    std::wstring winTz = ScheduleService::MapIanaToWindowsTimeZone(ianaTz);
    if (winTz.empty() && nearest) winTz = nearest->timeZoneId;
    if (winTz.empty()) winTz = GetSystemTimeZoneId();

    DetectedLocationResult r;
    r.cityId = nearest ? nearest->id : L"custom";
    r.cityName = !city.empty() ? Utf8ToWide(city) : (nearest ? nearest->name : L"Konum");
    r.countryName = !country.empty() ? Utf8ToWide(country) : (nearest ? nearest->country : L"Konum");
    r.countryCode = !cc.empty() ? Utf8ToWide(cc) : (nearest ? nearest->countryCode : ResolveCountryCodeFromName(r.countryName));
    if (r.countryCode.empty()) r.countryCode = GetSystemRegionCountryCode();
    r.latitude = lat;
    r.longitude = lon;
    r.timeZoneId = std::move(winTz);
    r.recommendedMethod = GetRecommendedMethodForCountry(r.countryCode);
    r.sourceLabel = sourceLabel ? sourceLabel : L"IP Konumu";
    return r;
}

} // namespace

std::optional<DetectedLocationResult> ScheduleService::ParseReverseGeoResponse(double lat, double lon, std::string_view jsonBody) {
    if (!IsValidCoordinates(lat, lon)) return std::nullopt;
    auto doc = JsonParse(jsonBody);
    if (!doc || doc->type != JsonValue::Type::Object) return std::nullopt;

    std::string city = CleanGeoField(doc->GetString("city"));
    if (city.empty()) city = CleanGeoField(doc->GetString("locality"));
    if (city.empty()) city = CleanGeoField(doc->GetString("principalSubdivision"));
    std::string country = CleanGeoField(doc->GetString("countryName"));
    std::string cc = CleanGeoField(doc->GetString("countryCode"));
    if (city.empty() && country.empty() && cc.empty()) return std::nullopt;

    return BuildDetectedLocation(lat, lon, city, country, cc, "", L"Windows Konum");
}

std::optional<DetectedLocationResult> ScheduleService::ParseIpGeoResponse(std::string_view providerKey, std::string_view jsonBody) {
    auto doc = JsonParse(jsonBody);
    if (!doc || doc->type != JsonValue::Type::Object) return std::nullopt;

    if (providerKey == "ipwho") {
        if (const JsonValue* okVal = doc->Find("success"); okVal && okVal->type == JsonValue::Type::Bool && !okVal->boolVal) {
            return std::nullopt;
        }
        double lat = doc->GetDouble("latitude", 999.0);
        double lon = doc->GetDouble("longitude", 999.0);
        if (!IsValidCoordinates(lat, lon)) return std::nullopt;
        std::string tz;
        if (const JsonValue* tzObj = doc->Find("timezone")) {
            tz = tzObj->GetString("id");
        }
        return BuildDetectedLocation(lat, lon, doc->GetString("city"), doc->GetString("country"),
                                     doc->GetString("country_code"), tz, L"IP Konumu (ipwho.is)");
    }

    if (providerKey == "freeipapi") {
        double lat = doc->GetDouble("latitude", 999.0);
        double lon = doc->GetDouble("longitude", 999.0);
        if (!IsValidCoordinates(lat, lon)) return std::nullopt;
        std::string tz;
        if (const JsonValue* tzArr = doc->Find("timeZones"); tzArr && tzArr->type == JsonValue::Type::Array && !tzArr->arrVal.empty()) {
            if (tzArr->arrVal[0].type == JsonValue::Type::String) {
                tz = tzArr->arrVal[0].strVal;
            }
        }
        return BuildDetectedLocation(lat, lon, doc->GetString("cityName"), doc->GetString("countryName"),
                                     doc->GetString("countryCode"), tz, L"IP Konumu (FreeIPAPI)");
    }

    if (providerKey == "ipapi") {
        if (doc->GetBool("error", false)) return std::nullopt;
        double lat = doc->GetDouble("latitude", 999.0);
        double lon = doc->GetDouble("longitude", 999.0);
        if (!IsValidCoordinates(lat, lon)) return std::nullopt;
        return BuildDetectedLocation(lat, lon, doc->GetString("city"), doc->GetString("country_name"),
                                     doc->GetString("country_code"), doc->GetString("timezone"), L"IP Konumu (ipapi.co)");
    }

    if (providerKey == "ipinfo") {
        std::string loc = doc->GetString("loc");
        size_t comma = loc.find(',');
        if (comma == std::string::npos) return std::nullopt;
        std::string latStr = loc.substr(0, comma);
        std::string lonStr = loc.substr(comma + 1);
        char* end1 = nullptr;
        char* end2 = nullptr;
        double lat = std::strtod(latStr.c_str(), &end1);
        double lon = std::strtod(lonStr.c_str(), &end2);
        if (!end1 || end1 == latStr.c_str() || !end2 || end2 == lonStr.c_str()) return std::nullopt;
        if (!IsValidCoordinates(lat, lon)) return std::nullopt;

        std::string cc = CleanGeoField(doc->GetString("country"));
        std::string countryName = cc;
        if (auto nearest = FindNearestCity(lat, lon, 120.0); nearest && !nearest->country.empty()) {
            std::string nearCc = WideToUtf8(nearest->countryCode);
            if (cc.empty() || nearCc == cc) {
                countryName = WideToUtf8(nearest->country);
            }
        }
        return BuildDetectedLocation(lat, lon, doc->GetString("city"), countryName,
                                     cc, doc->GetString("timezone"), L"IP Konumu (ipinfo.io)");
    }

    if (providerKey == "geojs") {
        double lat = doc->GetDouble("latitude", 999.0);
        double lon = doc->GetDouble("longitude", 999.0);
        if (!IsValidCoordinates(lat, lon)) return std::nullopt;
        return BuildDetectedLocation(lat, lon, doc->GetString("city"), doc->GetString("country"),
                                     doc->GetString("country_code"), doc->GetString("timezone"), L"IP Konumu (GeoJS)");
    }

    return std::nullopt;
}

std::optional<DetectedLocationResult> ScheduleService::DetectCurrentLocation() {
    auto windowsFix = GetWindowsLocationCoordinates();
    if (windowsFix) {
        double lat = windowsFix->latitude;
        double lon = windowsFix->longitude;

        if (auto nearest = FindNearestCity(lat, lon, 30.0)) {
            DetectedLocationResult result;
            result.cityId = nearest->id;
            result.cityName = nearest->name;
            result.countryName = nearest->country;
            result.countryCode = nearest->countryCode;
            result.latitude = lat;
            result.longitude = lon;
            result.timeZoneId = nearest->timeZoneId;
            result.recommendedMethod = GetRecommendedMethodForCountry(result.countryCode);
            result.sourceLabel = L"Windows Konum";
            return result;
        }

        wchar_t revPath[160];
        swprintf_s(revPath, L"/data/reverse-geocode-client?latitude=%.4f&longitude=%.4f&localityLanguage=en", lat, lon);
        if (auto body = WinHttpGetUtf8(L"api.bigdatacloud.net", revPath, true, 5000)) {
            if (auto parsed = ParseReverseGeoResponse(lat, lon, *body)) {
                return parsed;
            }
        }

        if (!windowsFix->civicCity.empty()) {
            std::wstring cc = ResolveCountryCodeFromName(windowsFix->civicCountry);
            if (cc.empty()) cc = GetSystemRegionCountryCode();
            DetectedLocationResult result;
            result.cityId = L"custom";
            result.cityName = windowsFix->civicCity;
            result.countryName = !windowsFix->civicCountry.empty() ? windowsFix->civicCountry : L"Konum";
            result.countryCode = cc;
            result.latitude = lat;
            result.longitude = lon;
            result.timeZoneId = GetSystemTimeZoneId();
            result.recommendedMethod = GetRecommendedMethodForCountry(cc);
            result.sourceLabel = L"Windows Konum";
            return result;
        }

        auto nearby = FindNearestCity(lat, lon, 120.0);
        std::wstring cc = nearby ? nearby->countryCode : GetSystemRegionCountryCode();
        wchar_t coordName[64];
        swprintf_s(coordName, L"%.2f, %.2f", lat, lon);

        DetectedLocationResult result;
        result.cityId = L"custom";
        result.cityName = nearby ? nearby->name : std::wstring(coordName);
        result.countryName = nearby ? nearby->country : (!cc.empty() ? cc : L"Konum");
        result.countryCode = cc;
        result.latitude = lat;
        result.longitude = lon;
        result.timeZoneId = (nearby && !nearby->timeZoneId.empty()) ? nearby->timeZoneId : GetSystemTimeZoneId();
        result.recommendedMethod = GetRecommendedMethodForCountry(cc);
        result.sourceLabel = L"Windows Konum";
        return result;
    }

    struct IpProvider {
        const char* key;
        const wchar_t* host;
        const wchar_t* path;
    };
    static constexpr IpProvider kProviders[] = {
        {"ipwho",     L"ipwho.is",         L"/"},
        {"freeipapi", L"free.freeipapi.com", L"/api/json"},
        {"ipapi",     L"ipapi.co",         L"/json/"},
        {"ipinfo",    L"ipinfo.io",        L"/json"},
        {"geojs",     L"get.geojs.io",     L"/v1/ip/geo.json"}
    };

    for (const auto& p : kProviders) {
        if (auto body = WinHttpGetUtf8(p.host, p.path, true, 5000)) {
            if (auto parsed = ParseIpGeoResponse(p.key, *body)) {
                return parsed;
            }
        }
    }

    return std::nullopt;
}

std::vector<CityLocation> ScheduleService::SearchWorldwideCities(std::wstring_view query) {
    std::wstring trimmed = TrimWide(query);
    if (trimmed.size() < 3) return {};

    std::string path = "/v1/search?name=" + EncodeQueryParameter(WideToUtf8(trimmed)) +
                       "&count=20&language=en&format=json";
    auto body = WinHttpGetUtf8(L"geocoding-api.open-meteo.com", Utf8ToWide(path), true, 6000);
    if (!body) return {};

    auto doc = JsonParse(*body);
    if (!doc || doc->type != JsonValue::Type::Object) return {};
    const JsonValue* results = doc->Find("results");
    if (!results || results->type != JsonValue::Type::Array) return {};

    std::vector<CityLocation> cities;
    cities.reserve(results->arrVal.size());
    for (const auto& item : results->arrVal) {
        if (item.type != JsonValue::Type::Object) continue;
        std::string name = item.GetString("name");
        std::string country = item.GetString("country");
        std::string countryCode = item.GetString("country_code");
        double latitude = item.GetDouble("latitude", 999.0);
        double longitude = item.GetDouble("longitude", 999.0);
        if (name.empty() || country.empty() || countryCode.empty() ||
            latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
            continue;
        }

        char idBuf[96];
        snprintf(idBuf, sizeof(idBuf), "world_%s_%d_%d", countryCode.c_str(),
                 static_cast<int>(std::llround(latitude * 10000.0)),
                 static_cast<int>(std::llround(longitude * 10000.0)));
        std::wstring timeZoneId = MapIanaToWindowsTimeZone(item.GetString("timezone"));
        if (timeZoneId.empty()) timeZoneId = GetSystemTimeZoneId();

        CityLocation city;
        city.id = Utf8ToWide(idBuf);
        city.name = Utf8ToWide(name);
        city.country = Utf8ToWide(country);
        city.countryCode = Utf8ToWide(countryCode);
        city.latitude = latitude;
        city.longitude = longitude;
        city.timeZoneId = std::move(timeZoneId);
        if (const JsonValue* admin = item.Find("admin1"); admin && admin->type == JsonValue::Type::String) {
            city.searchAliases = Utf8ToWide(admin->strVal);
        }
        cities.push_back(std::move(city));
    }
    return cities;
}

} // namespace vakit
