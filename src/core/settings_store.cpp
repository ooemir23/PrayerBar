#include "settings_store.h"
#include "json_mini.h"
#include "utf_util.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <filesystem>

namespace vakit {

std::wstring GetDefaultLocalAppDataDir() {
    static constexpr KNOWNFOLDERID kFolderIdLocalAppData = {
        0xF1B32785u, 0x6FBAu, 0x4FCFu, {0x9Du, 0x55u, 0x7Bu, 0x8Eu, 0x7Fu, 0x15u, 0x70u, 0x91u}
    };
    PWSTR pathPtr = nullptr;
    std::wstring base;
    if (SUCCEEDED(SHGetKnownFolderPath(kFolderIdLocalAppData, 0, nullptr, &pathPtr)) && pathPtr) {
        base = pathPtr;
        CoTaskMemFree(pathPtr);
    } else {
        wchar_t envBuf[MAX_PATH]{};
        DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", envBuf, MAX_PATH);
        if (len > 0 && len < MAX_PATH) base = envBuf;
    }
    if (base.empty()) base = L".";
    return base + L"\\VakitCubugu";
}

void EnsureDirectoryRecursive(const std::wstring& dir) {
    if (dir.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(dir), ec);
}

bool WriteFileAtomicallyUtf8(const std::wstring& path, const std::string& content) {
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        EnsureDirectoryRecursive(p.parent_path().wstring());
    }

    std::wstring tmpPath = path + L".tmp";
    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    BOOL ok = content.empty() ? TRUE : WriteFile(hFile, content.data(), static_cast<DWORD>(content.size()), &written, nullptr);
    CloseHandle(hFile);
    if (!ok || written != content.size()) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    return MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

bool ReadFileUtf8(const std::wstring& path, std::string& outContent) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart < 0 || sz.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(hFile);
        return false;
    }

    outContent.resize(static_cast<size_t>(sz.QuadPart));
    DWORD totalRead = 0;
    while (totalRead < outContent.size()) {
        DWORD chunk = 0;
        if (!ReadFile(hFile, outContent.data() + totalRead, static_cast<DWORD>(outContent.size() - totalRead), &chunk, nullptr) || chunk == 0) {
            break;
        }
        totalRead += chunk;
    }
    CloseHandle(hFile);
    outContent.resize(totalRead);
    return true;
}

namespace {

const char* AlertKeyName(PrayerKind kind) {
    switch (kind) {
        case PrayerKind::Imsak: return "imsak";
        case PrayerKind::Gunes: return "gunes";
        case PrayerKind::Ogle: return "ogle";
        case PrayerKind::Ikindi: return "ikindi";
        case PrayerKind::Aksam: return "aksam";
        case PrayerKind::Yatsi: return "yatsi";
    }
    return "imsak";
}

JsonValue SerializeRule(const PrayerAlertRule& r) {
    JsonValue o = JsonValue::MakeObject();
    o.Set("notifyAtTime", JsonValue::MakeBool(r.notifyAtTime));
    o.Set("preNotifyMinutes", JsonValue::MakeInt(r.preNotifyMinutes));
    o.Set("playSound", JsonValue::MakeBool(r.playSound));
    return o;
}

void DeserializeRule(const JsonValue* obj, PrayerAlertRule& r) {
    if (!obj || obj->type != JsonValue::Type::Object) return;
    r.notifyAtTime = obj->GetBool("notifyAtTime", r.notifyAtTime);
    r.preNotifyMinutes = std::clamp(obj->GetInt("preNotifyMinutes", r.preNotifyMinutes), 0, 120);
    r.playSound = obj->GetBool("playSound", r.playSound);
}

bool ParseSettingsUtf8(const std::string& raw, AppSettings& s) {
    auto parsed = JsonParse(raw);
    if (!parsed || parsed->type != JsonValue::Type::Object) return false;
    const JsonValue& root = *parsed;

    auto readWStr = [&](const char* key, std::wstring& dst) {
        const JsonValue* v = root.Find(key);
        if (v && v->type == JsonValue::Type::String) {
            dst = Utf8ToWide(v->strVal);
        }
    };

    readWStr("cityId", s.cityId);
    readWStr("cityName", s.cityName);
    readWStr("countryName", s.countryName);
    s.latitude = std::clamp(root.GetDouble("latitude", s.latitude), -70.0, 70.0);
    s.longitude = std::clamp(root.GetDouble("longitude", s.longitude), -180.0, 180.0);
    readWStr("timeZoneId", s.timeZoneId);
    s.useCustomCoordinates = root.GetBool("useCustomCoordinates", s.useCustomCoordinates);

    s.calculationMethod = static_cast<CalculationMethod>(root.GetInt("calculationMethod", static_cast<int>(s.calculationMethod)));
    s.asrMethod = static_cast<AsrJuristicMethod>(std::clamp(root.GetInt("asrMethod", static_cast<int>(s.asrMethod)), 0, 1));
    s.autoSelectMethodByCountry = root.GetBool("autoSelectMethodByCountry", s.autoSelectMethodByCountry);
    s.useOnlineSync = root.GetBool("useOnlineSync", s.useOnlineSync);

    s.showTaskbarPill = root.GetBool("showTaskbarPill", s.showTaskbarPill);
    s.showTrayIcon = root.GetBool("showTrayIcon", s.showTrayIcon);
    if (!s.showTaskbarPill && !s.showTrayIcon) {
        s.showTrayIcon = true; // Kullanıcının erişimden kilitlenmesini önle
    }
    s.displayStyle = static_cast<TaskbarDisplayStyle>(std::clamp(root.GetInt("displayStyle", static_cast<int>(s.displayStyle)), 0, 8));
    s.trayStyle = static_cast<TrayIconStyle>(std::clamp(root.GetInt("trayStyle", static_cast<int>(s.trayStyle)), 0, 3));
    readWStr("customTemplate", s.customTemplate);
    s.showProgressBar = root.GetBool("showProgressBar", s.showProgressBar);
    s.highlightUrgentMinutes = std::clamp(root.GetInt("highlightUrgentMinutes", s.highlightUrgentMinutes), 0, 60);
    s.showSeconds = root.GetBool("showSeconds", s.showSeconds);

    s.dockPosition = static_cast<TaskbarDockPosition>(std::clamp(root.GetInt("dockPosition", static_cast<int>(s.dockPosition)), 0, 3));
    s.horizontalOffsetPx = std::clamp(root.GetInt("horizontalOffsetPx", s.horizontalOffsetPx), -1000, 1000);
    s.opaqueTaskbarBackground = root.GetBool("opaqueTaskbarBackground", s.opaqueTaskbarBackground);
    s.fontSize = static_cast<FontSizePreset>(std::clamp(root.GetInt("fontSize", static_cast<int>(s.fontSize)), 0, 2));
    s.themeMode = static_cast<AppThemeMode>(std::clamp(root.GetInt("themeMode", static_cast<int>(s.themeMode)), 0, 2));
    s.useSystemAccentColor = root.GetBool("useSystemAccentColor", s.useSystemAccentColor);
    s.flyoutDesign = static_cast<FlyoutDesignStyle>(std::clamp(root.GetInt("flyoutDesign", static_cast<int>(s.flyoutDesign)), 0, 3));
    s.language = static_cast<AppLanguage>(std::clamp(root.GetInt("language", static_cast<int>(s.language)), 0, 12));

    if (const JsonValue* namesObj = root.Find("customPrayerNames"); namesObj && namesObj->type == JsonValue::Type::Object) {
        for (PrayerKind kind : kOrderedPrayerKinds) {
            const JsonValue* nv = namesObj->Find(AlertKeyName(kind));
            if (nv && nv->type == JsonValue::Type::String) {
                s.customPrayerNames[static_cast<size_t>(kind)] = Utf8ToWide(nv->strVal);
            }
        }
    }
    if (const JsonValue* shortsObj = root.Find("customPrayerShortCodes"); shortsObj && shortsObj->type == JsonValue::Type::Object) {
        for (PrayerKind kind : kOrderedPrayerKinds) {
            const JsonValue* sv = shortsObj->Find(AlertKeyName(kind));
            if (sv && sv->type == JsonValue::Type::String) {
                s.customPrayerShortCodes[static_cast<size_t>(kind)] = Utf8ToWide(sv->strVal);
            }
        }
    }

    s.notifyOnPrayerTime = root.GetBool("notifyOnPrayerTime", s.notifyOnPrayerTime);
    s.preNotifyMinutes = std::clamp(root.GetInt("preNotifyMinutes", s.preNotifyMinutes), 0, 120);
    s.playNotificationSound = root.GetBool("playNotificationSound", s.playNotificationSound);
    s.startWithWindows = root.GetBool("startWithWindows", s.startWithWindows);
    s.autoOptimizeMemory = root.GetBool("autoOptimizeMemory", s.autoOptimizeMemory);
    s.quietDuringFullscreen = root.GetBool("quietDuringFullscreen", s.quietDuringFullscreen);
    s.showKerahatIndicator = root.GetBool("showKerahatIndicator", s.showKerahatIndicator);
    s.ramadanIftarSahurMode = root.GetBool("ramadanIftarSahurMode", s.ramadanIftarSahurMode);
    s.fridayReminder = root.GetBool("fridayReminder", s.fridayReminder);
    s.reviewPrompted = root.GetBool("reviewPrompted", s.reviewPrompted);
    s.notifyMuteUntilMin = std::max(0, root.GetInt("notifyMuteUntilMin", 0));
    s.fajrSound = std::clamp(root.GetInt("fajrSound", s.fajrSound), -1, 5);
    s.holidayReminders = root.GetBool("holidayReminders", s.holidayReminders);
    s.showExtraTimes = root.GetBool("showExtraTimes", s.showExtraTimes);
    s.travelMode = root.GetBool("travelMode", s.travelMode);
    s.prayerTracking = root.GetBool("prayerTracking", s.prayerTracking);
    s.modernToasts = root.GetBool("modernToasts", s.modernToasts);
    s.globalHotkey = root.GetBool("globalHotkey", s.globalHotkey);
    s.hotkeyMods = root.GetInt("hotkeyMods", s.hotkeyMods) & 0x000F;
    s.hotkeyVk = std::clamp(root.GetInt("hotkeyVk", s.hotkeyVk), 0x08, 0xFE);
    if (s.hotkeyMods == 0) s.hotkeyMods = 0x0009;  // never register a bare key globally
    s.lastTzHash = root.GetInt("lastTzHash", s.lastTzHash);

    s.entrySound = static_cast<NotificationSoundKind>(std::clamp(root.GetInt("entrySound", static_cast<int>(s.entrySound)), 0, 5));
    s.preNotifySound = static_cast<NotificationSoundKind>(std::clamp(root.GetInt("preNotifySound", static_cast<int>(s.preNotifySound)), 0, 5));
    readWStr("customSoundPath", s.customSoundPath);
    s.soundVolumePercent = std::clamp(root.GetInt("soundVolumePercent", s.soundVolumePercent), 0, 100);

    if (const JsonValue* alertsObj = root.Find("alerts"); alertsObj && alertsObj->type == JsonValue::Type::Object) {
        for (PrayerKind kind : kOrderedPrayerKinds) {
            DeserializeRule(alertsObj->Find(AlertKeyName(kind)), s.alerts.GetRule(kind));
        }
    } else {
        for (PrayerKind kind : kOrderedPrayerKinds) {
            auto& rule = s.alerts.GetRule(kind);
            rule.notifyAtTime = s.notifyOnPrayerTime;
            rule.preNotifyMinutes = s.preNotifyMinutes;
            rule.playSound = s.playNotificationSound;
        }
    }

    if (const JsonValue* offObj = root.Find("offsets"); offObj && offObj->type == JsonValue::Type::Object) {
        s.offsets.imsak = std::clamp(offObj->GetInt("imsak", 0), -60, 60);
        s.offsets.gunes = std::clamp(offObj->GetInt("gunes", 0), -60, 60);
        s.offsets.ogle = std::clamp(offObj->GetInt("ogle", 0), -60, 60);
        s.offsets.ikindi = std::clamp(offObj->GetInt("ikindi", 0), -60, 60);
        s.offsets.aksam = std::clamp(offObj->GetInt("aksam", 0), -60, 60);
        s.offsets.yatsi = std::clamp(offObj->GetInt("yatsi", 0), -60, 60);
    }

    return true;
}

std::string SerializeSettingsUtf8(const AppSettings& s) {
    JsonValue root = JsonValue::MakeObject();
    root.Set("cityId", JsonValue::MakeString(WideToUtf8(s.cityId)));
    root.Set("cityName", JsonValue::MakeString(WideToUtf8(s.cityName)));
    root.Set("countryName", JsonValue::MakeString(WideToUtf8(s.countryName)));
    root.Set("latitude", JsonValue::MakeNumber(s.latitude));
    root.Set("longitude", JsonValue::MakeNumber(s.longitude));
    root.Set("timeZoneId", JsonValue::MakeString(WideToUtf8(s.timeZoneId)));
    root.Set("useCustomCoordinates", JsonValue::MakeBool(s.useCustomCoordinates));

    root.Set("calculationMethod", JsonValue::MakeInt(static_cast<int>(s.calculationMethod)));
    root.Set("asrMethod", JsonValue::MakeInt(static_cast<int>(s.asrMethod)));
    root.Set("autoSelectMethodByCountry", JsonValue::MakeBool(s.autoSelectMethodByCountry));
    root.Set("useOnlineSync", JsonValue::MakeBool(s.useOnlineSync));

    root.Set("showTaskbarPill", JsonValue::MakeBool(s.showTaskbarPill));
    root.Set("showTrayIcon", JsonValue::MakeBool(s.showTrayIcon));
    root.Set("displayStyle", JsonValue::MakeInt(static_cast<int>(s.displayStyle)));
    root.Set("trayStyle", JsonValue::MakeInt(static_cast<int>(s.trayStyle)));
    root.Set("customTemplate", JsonValue::MakeString(WideToUtf8(s.customTemplate)));
    root.Set("showProgressBar", JsonValue::MakeBool(s.showProgressBar));
    root.Set("highlightUrgentMinutes", JsonValue::MakeInt(s.highlightUrgentMinutes));
    root.Set("showSeconds", JsonValue::MakeBool(s.showSeconds));

    root.Set("dockPosition", JsonValue::MakeInt(static_cast<int>(s.dockPosition)));
    root.Set("horizontalOffsetPx", JsonValue::MakeInt(s.horizontalOffsetPx));
    root.Set("opaqueTaskbarBackground", JsonValue::MakeBool(s.opaqueTaskbarBackground));
    root.Set("fontSize", JsonValue::MakeInt(static_cast<int>(s.fontSize)));
    root.Set("themeMode", JsonValue::MakeInt(static_cast<int>(s.themeMode)));
    root.Set("useSystemAccentColor", JsonValue::MakeBool(s.useSystemAccentColor));
    root.Set("flyoutDesign", JsonValue::MakeInt(static_cast<int>(s.flyoutDesign)));
    root.Set("language", JsonValue::MakeInt(static_cast<int>(s.language)));

    JsonValue namesObj = JsonValue::MakeObject();
    JsonValue shortsObj = JsonValue::MakeObject();
    for (PrayerKind kind : kOrderedPrayerKinds) {
        size_t idx = static_cast<size_t>(kind);
        namesObj.Set(AlertKeyName(kind), JsonValue::MakeString(WideToUtf8(s.customPrayerNames[idx])));
        shortsObj.Set(AlertKeyName(kind), JsonValue::MakeString(WideToUtf8(s.customPrayerShortCodes[idx])));
    }
    root.Set("customPrayerNames", std::move(namesObj));
    root.Set("customPrayerShortCodes", std::move(shortsObj));

    root.Set("notifyOnPrayerTime", JsonValue::MakeBool(s.notifyOnPrayerTime));
    root.Set("preNotifyMinutes", JsonValue::MakeInt(s.preNotifyMinutes));
    root.Set("playNotificationSound", JsonValue::MakeBool(s.playNotificationSound));
    root.Set("startWithWindows", JsonValue::MakeBool(s.startWithWindows));
    root.Set("autoOptimizeMemory", JsonValue::MakeBool(s.autoOptimizeMemory));
    root.Set("quietDuringFullscreen", JsonValue::MakeBool(s.quietDuringFullscreen));
    root.Set("showKerahatIndicator", JsonValue::MakeBool(s.showKerahatIndicator));
    root.Set("ramadanIftarSahurMode", JsonValue::MakeBool(s.ramadanIftarSahurMode));
    root.Set("fridayReminder", JsonValue::MakeBool(s.fridayReminder));
    root.Set("reviewPrompted", JsonValue::MakeBool(s.reviewPrompted));
    root.Set("notifyMuteUntilMin", JsonValue::MakeInt(s.notifyMuteUntilMin));
    root.Set("fajrSound", JsonValue::MakeInt(s.fajrSound));
    root.Set("holidayReminders", JsonValue::MakeBool(s.holidayReminders));
    root.Set("showExtraTimes", JsonValue::MakeBool(s.showExtraTimes));
    root.Set("travelMode", JsonValue::MakeBool(s.travelMode));
    root.Set("prayerTracking", JsonValue::MakeBool(s.prayerTracking));
    root.Set("modernToasts", JsonValue::MakeBool(s.modernToasts));
    root.Set("globalHotkey", JsonValue::MakeBool(s.globalHotkey));
    root.Set("hotkeyMods", JsonValue::MakeInt(s.hotkeyMods));
    root.Set("hotkeyVk", JsonValue::MakeInt(s.hotkeyVk));
    root.Set("lastTzHash", JsonValue::MakeInt(s.lastTzHash));

    root.Set("entrySound", JsonValue::MakeInt(static_cast<int>(s.entrySound)));
    root.Set("preNotifySound", JsonValue::MakeInt(static_cast<int>(s.preNotifySound)));
    root.Set("customSoundPath", JsonValue::MakeString(WideToUtf8(s.customSoundPath)));
    root.Set("soundVolumePercent", JsonValue::MakeInt(s.soundVolumePercent));

    JsonValue alertsObj = JsonValue::MakeObject();
    for (PrayerKind kind : kOrderedPrayerKinds) {
        alertsObj.Set(AlertKeyName(kind), SerializeRule(s.alerts.GetRule(kind)));
    }
    root.Set("alerts", std::move(alertsObj));

    JsonValue offObj = JsonValue::MakeObject();
    offObj.Set("imsak", JsonValue::MakeInt(s.offsets.imsak));
    offObj.Set("gunes", JsonValue::MakeInt(s.offsets.gunes));
    offObj.Set("ogle", JsonValue::MakeInt(s.offsets.ogle));
    offObj.Set("ikindi", JsonValue::MakeInt(s.offsets.ikindi));
    offObj.Set("aksam", JsonValue::MakeInt(s.offsets.aksam));
    offObj.Set("yatsi", JsonValue::MakeInt(s.offsets.yatsi));
    root.Set("offsets", std::move(offObj));

    return JsonSerialize(root, true);
}

} // namespace

SettingsStore::SettingsStore(std::wstring customBaseDir)
    : baseDir_(customBaseDir.empty() ? GetDefaultLocalAppDataDir() : std::move(customBaseDir)) {
    settingsPath_ = baseDir_ + L"\\settings.json";
}

AppSettings SettingsStore::Load() const {
    AppSettings s;
    std::string raw;
    if (!ReadFileUtf8(settingsPath_, raw)) return s;
    ParseSettingsUtf8(raw, s);
    return s;
}

bool SettingsStore::Save(const AppSettings& s) const {
    AppSettings copy = s;
    if (!copy.showTaskbarPill && !copy.showTrayIcon) {
        copy.showTrayIcon = true;
    }
    return WriteFileAtomicallyUtf8(settingsPath_, SerializeSettingsUtf8(copy));
}

bool SettingsStore::ExportToFile(const AppSettings& settings, const std::wstring& filePath) const {
    return WriteFileAtomicallyUtf8(filePath, SerializeSettingsUtf8(settings));
}

std::optional<AppSettings> SettingsStore::ImportFromFile(const std::wstring& filePath) const {
    std::string raw;
    if (!ReadFileUtf8(filePath, raw)) return std::nullopt;
    AppSettings s;
    if (!ParseSettingsUtf8(raw, s)) return std::nullopt;
    return s;
}

} // namespace vakit
