#include "theme_utils.h"

#include <dwmapi.h>
#include <psapi.h>

namespace vakit {

namespace {

constexpr wchar_t kPersonalizeKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"PrayerBar";

int ReadPersonalizeDword(const wchar_t* valueName, int defVal) {
    DWORD val = 0;
    DWORD size = sizeof(val);
    LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, kPersonalizeKey, valueName, RRF_RT_REG_DWORD, nullptr, &val, &size);
    return (rc == ERROR_SUCCESS) ? static_cast<int>(val) : defVal;
}

} // namespace

bool IsTaskbarLight(AppThemeMode mode) {
    if (mode == AppThemeMode::Light) return true;
    if (mode == AppThemeMode::Dark) return false;
    return ReadPersonalizeDword(L"SystemUsesLightTheme", 0) != 0;
}

bool IsAppLight(AppThemeMode mode) {
    if (mode == AppThemeMode::Light) return true;
    if (mode == AppThemeMode::Dark) return false;
    return ReadPersonalizeDword(L"AppsUseLightTheme", 0) != 0;
}

bool IsBatterySaverActive() {
    SYSTEM_POWER_STATUS sps{};
    if (GetSystemPowerStatus(&sps)) {
        return sps.SystemStatusFlag != 0;
    }
    return false;
}

bool IsQuietOrFullscreenModeActive() {
    using QueryStateFn = HRESULT(WINAPI*)(int*);
    static QueryStateFn fn = []() -> QueryStateFn {
        HMODULE sh32 = GetModuleHandleW(L"shell32.dll");
        if (!sh32) return nullptr;
        return reinterpret_cast<QueryStateFn>(reinterpret_cast<void*>(GetProcAddress(sh32, "SHQueryUserNotificationState")));
    }();
    if (!fn) return false;
    int state = 0;
    if (FAILED(fn(&state))) return false;
    // 2=BUSY (tam ekran), 3=D3D_FULL_SCREEN (oyun), 4=PRESENTATION_MODE, 7=QUIET_TIME
    return state == 2 || state == 3 || state == 4 || state == 7;
}

namespace {

DWORD GetWindowsBuildNumber() {
    static DWORD build = []() -> DWORD {
        using RtlGetVerFn = LONG(WINAPI*)(OSVERSIONINFOW*);
        HMODULE nt = GetModuleHandleW(L"ntdll.dll");
        if (nt) {
            auto fn = reinterpret_cast<RtlGetVerFn>(reinterpret_cast<void*>(GetProcAddress(nt, "RtlGetVersion")));
            if (fn) {
                OSVERSIONINFOW vi{};
                vi.dwOSVersionInfoSize = sizeof(vi);
                if (fn(&vi) == 0) return vi.dwBuildNumber;
            }
        }
        return 0;
    }();
    return build;
}

int CALLBACK EnumFontCallback(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM lParam) {
    *reinterpret_cast<bool*>(lParam) = true;
    return 0;
}

bool HasFontFamily(HDC dc, const wchar_t* name) {
    bool found = false;
    LOGFONTW lf{};
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName, name, LF_FACESIZE);
    EnumFontFamiliesExW(dc, &lf, EnumFontCallback, reinterpret_cast<LPARAM>(&found), 0);
    return found;
}

} // namespace

void ApplyWin11AppDarkMode(AppThemeMode mode) {
    if (GetWindowsBuildNumber() < 18362) return;
    HMODULE uxt = GetModuleHandleW(L"uxtheme.dll");
    if (!uxt) uxt = LoadLibraryW(L"uxtheme.dll");
    if (!uxt) return;

    using SetPreferredAppModeFn = int(WINAPI*)(int);
    using FlushMenuThemesFn = void(WINAPI*)();
    static auto setModeFn = reinterpret_cast<SetPreferredAppModeFn>(
        reinterpret_cast<void*>(GetProcAddress(uxt, MAKEINTRESOURCEA(135))));
    static auto flushFn = reinterpret_cast<FlushMenuThemesFn>(
        reinterpret_cast<void*>(GetProcAddress(uxt, MAKEINTRESOURCEA(136))));

    int preferred = 1; // AllowDark
    if (mode == AppThemeMode::Dark) preferred = 2;      // ForceDark
    else if (mode == AppThemeMode::Light) preferred = 3; // ForceLight

    if (setModeFn) setModeFn(preferred);
    if (flushFn) flushFn();
}

void AllowDarkModeForWindowCompat(HWND hwnd, bool enable) {
    if (!hwnd || GetWindowsBuildNumber() < 18362) return;
    HMODULE uxt = GetModuleHandleW(L"uxtheme.dll");
    if (!uxt) uxt = LoadLibraryW(L"uxtheme.dll");
    if (!uxt) return;

    using AllowWindowFn = BOOL(WINAPI*)(HWND, BOOL);
    static auto fn = reinterpret_cast<AllowWindowFn>(
        reinterpret_cast<void*>(GetProcAddress(uxt, MAKEINTRESOURCEA(133))));
    if (fn) fn(hwnd, enable ? TRUE : FALSE);
}

namespace {
DWORD g_accentRawCached = 0xFFFFFFFFu;  // 0xFFFFFFFF = not read yet
}

void InvalidateAccentColorCache() {
    g_accentRawCached = 0xFFFFFFFFu;
}

uint32_t GetWindowsAccentColorArgb(bool isDark, bool useSystemAccent) {
    if (!useSystemAccent) {
        return isDark ? 0xFF38C9B2u : 0xFF0D7368u;
    }

    // Read once and cache: this runs on every paint. InvalidateAccentColorCache() runs on colour changes.
    if (g_accentRawCached == 0xFFFFFFFFu) {
        DWORD rawRead = 0;
        DWORD sz = sizeof(rawRead);
        LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor",
                                  RRF_RT_REG_DWORD, nullptr, &rawRead, &sz);
        g_accentRawCached = (rc == ERROR_SUCCESS) ? rawRead : 0;
    }
    DWORD raw = g_accentRawCached;
    if (raw == 0) {
        return isDark ? 0xFF38C9B2u : 0xFF0D7368u;
    }

    int r = static_cast<int>(raw & 0xFFu);
    int g = static_cast<int>((raw >> 8) & 0xFFu);
    int b = static_cast<int>((raw >> 16) & 0xFFu);

    double lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
    if (isDark && lum < 0.46) {
        r = std::min(255, static_cast<int>(r + (255 - r) * 0.42));
        g = std::min(255, static_cast<int>(g + (255 - g) * 0.42));
        b = std::min(255, static_cast<int>(b + (255 - b) * 0.42));
    } else if (!isDark && lum > 0.56) {
        r = static_cast<int>(r * 0.68);
        g = static_cast<int>(g * 0.68);
        b = static_cast<int>(b * 0.68);
    }

    return 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

COLORREF GetWindowsAccentColorRef(bool isDark, bool useSystemAccent) {
    uint32_t argb = GetWindowsAccentColorArgb(isDark, useSystemAccent);
    return RGB((argb >> 16) & 0xFFu, (argb >> 8) & 0xFFu, argb & 0xFFu);
}

const wchar_t* GetWin11UiFontFaceName(bool display) {
    static struct FontCache {
        bool hasDisplay = false;
        bool hasText = false;
    } cache = []() -> FontCache {
        FontCache c;
        HDC dc = GetDC(nullptr);
        if (dc) {
            c.hasDisplay = HasFontFamily(dc, L"Segoe UI Variable Display");
            c.hasText = HasFontFamily(dc, L"Segoe UI Variable Text");
            ReleaseDC(nullptr, dc);
        }
        return c;
    }();

    if (display && cache.hasDisplay) return L"Segoe UI Variable Display";
    if (cache.hasText) return L"Segoe UI Variable Text";
    if (cache.hasDisplay) return L"Segoe UI Variable Display";
    return L"Segoe UI";
}

HFONT CreateWin11Font(int pixelHeight, int weight, bool display) {
    return CreateFontW(
        pixelHeight, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, GetWin11UiFontFaceName(display));
}

bool IsPackagedApp() {
    using GetPkgFn = LONG(WINAPI*)(UINT32*, PWSTR);
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (!k32) return false;
    auto fn = reinterpret_cast<GetPkgFn>(reinterpret_cast<void*>(GetProcAddress(k32, "GetCurrentPackageFullName")));
    if (!fn) return false;
    UINT32 len = 0;
    LONG rc = fn(&len, nullptr);
    return rc != 15700L; // APPMODEL_ERROR_NO_PACKAGE
}

UINT GetDpiForWindowCompat(HWND hwnd) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static GetDpiForWindowFn fn = []() -> GetDpiForWindowFn {
        HMODULE u32 = GetModuleHandleW(L"user32.dll");
        if (!u32) return nullptr;
        return reinterpret_cast<GetDpiForWindowFn>(reinterpret_cast<void*>(GetProcAddress(u32, "GetDpiForWindow")));
    }();
    if (fn && hwnd) {
        UINT dpi = fn(hwnd);
        if (dpi >= 96) return dpi;
    }
    HDC screenDc = GetDC(nullptr);
    int dpiX = screenDc ? GetDeviceCaps(screenDc, LOGPIXELSX) : 96;
    if (screenDc) ReleaseDC(nullptr, screenDc);
    return dpiX >= 96 ? static_cast<UINT>(dpiX) : 96U;
}

void ApplyWindowDwmStyle(HWND hwnd, bool isDark, int cornerPref, int backdropType) {
    if (!hwnd) return;
    AllowDarkModeForWindowCompat(hwnd, isDark);

    BOOL darkVal = isDark ? TRUE : FALSE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE: 20 (Win10 20H1+/Win11), 19 (Win10 1903-1909)
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &darkVal, sizeof(darkVal)))) {
        DwmSetWindowAttribute(hwnd, 19, &darkVal, sizeof(darkVal));
    }

    // DWMWA_WINDOW_CORNER_PREFERENCE = 33 (2 = DWMWCP_ROUND)
    if (cornerPref > 0) {
        DwmSetWindowAttribute(hwnd, 33, &cornerPref, sizeof(cornerPref));
    }

    // Windows 11 DWM başlık ve kenarlık uyumu (34=BORDER, 35=CAPTION, 36=TEXT)
    COLORREF borderCol = isDark ? RGB(58, 58, 64) : RGB(214, 218, 224);
    COLORREF captionCol = isDark ? RGB(32, 32, 32) : RGB(243, 243, 243);
    COLORREF textCol = isDark ? RGB(245, 245, 245) : RGB(24, 24, 24);
    DwmSetWindowAttribute(hwnd, 34, &borderCol, sizeof(borderCol));
    DwmSetWindowAttribute(hwnd, 35, &captionCol, sizeof(captionCol));
    DwmSetWindowAttribute(hwnd, 36, &textCol, sizeof(textCol));

    // DWMWA_SYSTEMBACKDROP_TYPE = 38 (2 = Mica, 3 = Acrylic)
    if (backdropType > 0 && !IsBatterySaverActive()) {
        DwmSetWindowAttribute(hwnd, 38, &backdropType, sizeof(backdropType));
    }
}

bool IsStartupEnabled() {
    wchar_t buf[MAX_PATH * 2]{};
    DWORD size = sizeof(buf);
    LSTATUS rc = RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValueName, RRF_RT_REG_SZ, nullptr, buf, &size);
    return rc == ERROR_SUCCESS && buf[0] != L'\0';
}

bool SetStartupEnabled(bool enable, std::wstring& outNote) {
    HKEY hKey = nullptr;
    LSTATUS rc = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hKey);
    if (rc != ERROR_SUCCESS) {
        outNote = L"Başlangıç kayıt defteri anahtarı açılamadı.";
        return false;
    }

    bool ok = false;
    if (enable) {
        wchar_t exePath[MAX_PATH]{};
        DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            std::wstring quoted = std::wstring(L"\"") + exePath + L"\"";
            rc = RegSetValueExW(hKey, kRunValueName, 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(quoted.c_str()),
                                static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
            ok = (rc == ERROR_SUCCESS);
        }
    } else {
        rc = RegDeleteValueW(hKey, kRunValueName);
        ok = (rc == ERROR_SUCCESS || rc == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(hKey);
    if (!ok) {
        outNote = L"Başlangıç ayarı güncellenemedi.";
    } else {
        outNote.clear();
    }
    return ok;
}

void TrimWorkingSet() {
    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
}

MemoryStats GetMemoryStats() {
    MemoryStats stats;
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        stats.workingSetMb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        stats.privateMb = static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    }
    GetProcessHandleCount(GetCurrentProcess(), &stats.handleCount);
    return stats;
}

} // namespace vakit
