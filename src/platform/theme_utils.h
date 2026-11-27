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

struct MemoryStats {
    double workingSetMb = 0.0;
    double privateMb = 0.0;
    DWORD handleCount = 0;
};

bool IsTaskbarLight(AppThemeMode mode);
bool IsAppLight(AppThemeMode mode);
bool IsBatterySaverActive();
bool IsPackagedApp();
bool IsQuietOrFullscreenModeActive();

void ApplyWin11AppDarkMode(AppThemeMode mode);
void AllowDarkModeForWindowCompat(HWND hwnd, bool enable);
uint32_t GetWindowsAccentColorArgb(bool isDark, bool useSystemAccent);
void InvalidateAccentColorCache();
COLORREF GetWindowsAccentColorRef(bool isDark, bool useSystemAccent);
const wchar_t* GetWin11UiFontFaceName(bool display = false);
HFONT CreateWin11Font(int pixelHeight, int weight, bool display = false);

void ApplyWindowDwmStyle(HWND hwnd, bool isDark, int cornerPref = 2, int backdropType = 2);
UINT GetDpiForWindowCompat(HWND hwnd);
UINT GetDpiForMonitorCompat(HMONITOR mon);

bool IsStartupEnabled();
bool SetStartupEnabled(bool enable, std::wstring& outNote);
void MigrateLegacyPackagedStartupIfNeeded(bool settingsStartWithWindows);

void TrimWorkingSet();
MemoryStats GetMemoryStats();

} // namespace vakit
