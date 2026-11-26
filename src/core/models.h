#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace vakit {

enum class PrayerKind : int {
    Imsak = 0,
    Gunes = 1,
    Ogle = 2,
    Ikindi = 3,
    Aksam = 4,
    Yatsi = 5
};

inline constexpr std::array<PrayerKind, 6> kOrderedPrayerKinds = {
    PrayerKind::Imsak,
    PrayerKind::Gunes,
    PrayerKind::Ogle,
    PrayerKind::Ikindi,
    PrayerKind::Aksam,
    PrayerKind::Yatsi
};

inline const wchar_t* GetPrayerDisplayName(PrayerKind kind) {
    switch (kind) {
        case PrayerKind::Imsak: return L"İmsak";
        case PrayerKind::Gunes: return L"Güneş";
        case PrayerKind::Ogle: return L"Öğle";
        case PrayerKind::Ikindi: return L"İkindi";
        case PrayerKind::Aksam: return L"Akşam";
        case PrayerKind::Yatsi: return L"Yatsı";
    }
    return L"";
}

inline const wchar_t* GetPrayerDativeName(PrayerKind kind) {
    switch (kind) {
        case PrayerKind::Imsak: return L"İmsağa";
        case PrayerKind::Gunes: return L"Güneşe";
        case PrayerKind::Ogle: return L"Öğleye";
        case PrayerKind::Ikindi: return L"İkindiye";
        case PrayerKind::Aksam: return L"Akşama";
        case PrayerKind::Yatsi: return L"Yatsıya";
    }
    return L"";
}

inline const wchar_t* GetPrayerShortCode(PrayerKind kind) {
    switch (kind) {
        case PrayerKind::Imsak: return L"İm";
        case PrayerKind::Gunes: return L"G";
        case PrayerKind::Ogle: return L"Ö";
        case PrayerKind::Ikindi: return L"İ";
        case PrayerKind::Aksam: return L"A";
        case PrayerKind::Yatsi: return L"Y";
    }
    return L"";
}

inline uint32_t GetPrayerAccentArgb(PrayerKind kind) {
    switch (kind) {
        case PrayerKind::Imsak: return 0xFF64B5F6u;
        case PrayerKind::Gunes: return 0xFFFFB74Du;
        case PrayerKind::Ogle: return 0xFFFFD54Fu;
        case PrayerKind::Ikindi: return 0xFFFF9E40u;
        case PrayerKind::Aksam: return 0xFFFF7043u;
        case PrayerKind::Yatsi: return 0xFF9575CDu;
    }
    return 0xFF4DB6ACu;
}

enum class TaskbarDisplayStyle : int {
    TwoLineClassic = 0,
    CompactArrow = 1,
    CompactArrowSpaced = 2,
    TwoLineWithTime = 3,
    IconAndCountdown = 4,
    TwoLineShort = 5,
    MinimalDotTimer = 6,
    NextPrayerTime = 7,
    Custom = 8
};

enum class TrayIconStyle : int {
    TwoLineMini = 0,
    MinutesRing = 1,
    CrescentBadge = 2,
    Hidden = 3,
    LargeDigits = 4
};

enum class CalculationMethod : int {
    Jafari = 0,
    Karachi = 1,
    ISNA = 2,
    MWL = 3,
    UmmAlQura = 4,
    Egypt = 5,
    Tehran = 7,
    Gulf = 8,
    Kuwait = 9,
    Qatar = 10,
    Singapore = 11,
    FranceUOIF = 12,
    Diyanet = 13,
    Russia = 14,
    Moonsighting = 15,
    Dubai = 16,
    MalaysiaJAKIM = 17,
    Tunisia = 18,
    Algeria = 19,
    IndonesiaKEMENAG = 20,
    Morocco = 21,
    Portugal = 22,
    Jordan = 23
};

inline constexpr std::array<CalculationMethod, 23> kOrderedCalculationMethods = {
    CalculationMethod::Diyanet,
    CalculationMethod::MWL,
    CalculationMethod::ISNA,
    CalculationMethod::UmmAlQura,
    CalculationMethod::Egypt,
    CalculationMethod::Karachi,
    CalculationMethod::FranceUOIF,
    CalculationMethod::Moonsighting,
    CalculationMethod::Gulf,
    CalculationMethod::Kuwait,
    CalculationMethod::Qatar,
    CalculationMethod::Dubai,
    CalculationMethod::Singapore,
    CalculationMethod::MalaysiaJAKIM,
    CalculationMethod::IndonesiaKEMENAG,
    CalculationMethod::Russia,
    CalculationMethod::Tunisia,
    CalculationMethod::Algeria,
    CalculationMethod::Morocco,
    CalculationMethod::Jordan,
    CalculationMethod::Portugal,
    CalculationMethod::Tehran,
    CalculationMethod::Jafari
};

enum class AsrJuristicMethod : int {
    Standard = 0,
    Hanafi = 1
};

enum class NotificationSoundKind : int {
    None = 0,
    GentleChime = 1,
    WarmBell = 2,
    AdhanShort = 3,
    AdhanMakkah = 4,
    CustomFile = 5
};

enum class TaskbarDockPosition : int {
    RightNearTray = 0,
    FarRight = 1,
    Center = 2,
    LeftSide = 3
};

enum class FontSizePreset : int {
    Small = 0,
    Normal = 1,
    Large = 2
};

enum class AppThemeMode : int {
    System = 0,
    Dark = 1,
    Light = 2
};

enum class AppLanguage : int {
    Auto = 0,
    Turkish = 1,
    English = 2,
    Arabic = 3,
    German = 4,
    French = 5,
    Indonesian = 6,
    Urdu = 7,
    Bengali = 8,
    Malay = 9,
    Persian = 10,
    Russian = 11,
    Spanish = 12
};

enum class FlyoutDesignStyle : int {
    CompactFluent = 0,
    MinimalGlass = 1,
    CardGrid = 2,
    TimelineDashboard = 3
};

enum class TaskbarPillColorMode : int {
    Emerald = 0,
    ThemeAdaptive = 1,
    MinimalTransparent = 2
};

enum class KerahatStatus : int {
    None = 0,
    AfterSunrise = 1,
    BeforeDhuhr = 2,
    BeforeMaghrib = 3
};

enum class LicenseTier : int {
    Free = 0,
    Trial = 1,
    ProAnnual = 2,
    ProLifetime = 3
};

struct LicenseStatus {
    LicenseTier tier = LicenseTier::Trial;
    int trialDaysRemaining = 5;
    std::wstring maskedKey;
    std::wstring expiryIso;

    bool IsPro() const { return tier != LicenseTier::Free; }
};

struct PrayerOffsets {
    int imsak = 0;
    int gunes = 0;
    int ogle = 0;
    int ikindi = 0;
    int aksam = 0;
    int yatsi = 0;

    int GetOffset(PrayerKind kind) const {
        switch (kind) {
            case PrayerKind::Imsak: return imsak;
            case PrayerKind::Gunes: return gunes;
            case PrayerKind::Ogle: return ogle;
            case PrayerKind::Ikindi: return ikindi;
            case PrayerKind::Aksam: return aksam;
            case PrayerKind::Yatsi: return yatsi;
        }
        return 0;
    }

    bool IsAllZero() const {
        return imsak == 0 && gunes == 0 && ogle == 0 && ikindi == 0 && aksam == 0 && yatsi == 0;
    }
};

struct PrayerAlertRule {
    bool notifyAtTime = true;
    int preNotifyMinutes = 15;
    bool playSound = true;
};

struct PrayerAlertSchedule {
    std::array<PrayerAlertRule, 6> rules{};

    PrayerAlertRule& GetRule(PrayerKind kind) {
        return rules[static_cast<size_t>(kind)];
    }

    const PrayerAlertRule& GetRule(PrayerKind kind) const {
        return rules[static_cast<size_t>(kind)];
    }
};

struct AppSettings {
    std::wstring cityId = L"istanbul";
    std::wstring cityName = L"İstanbul";
    std::wstring countryName = L"Türkiye";
    double latitude = 41.0082;
    double longitude = 28.9784;
    std::wstring timeZoneId = L"Turkey Standard Time";
    bool useCustomCoordinates = false;
    bool autoDetectLocation = true;

    CalculationMethod calculationMethod = CalculationMethod::Diyanet;
    AsrJuristicMethod asrMethod = AsrJuristicMethod::Standard;
    bool autoSelectMethodByCountry = true;
    bool useOnlineSync = true;

    bool showTaskbarPill = true;
    bool showTrayIcon = true;
    TaskbarDisplayStyle displayStyle = TaskbarDisplayStyle::TwoLineClassic;
    TrayIconStyle trayStyle = TrayIconStyle::TwoLineMini;
    bool trayShowMinutesUpTo99 = false;
    std::wstring customTemplate = L"{kisa}→{kalan_kisa}";
    bool showProgressBar = true;
    int highlightUrgentMinutes = 15;
    bool showSeconds = false;

    TaskbarDockPosition dockPosition = TaskbarDockPosition::RightNearTray;
    int horizontalOffsetPx = 0;
    bool opaqueTaskbarBackground = false;
    FontSizePreset fontSize = FontSizePreset::Normal;
    AppThemeMode themeMode = AppThemeMode::System;
    bool useSystemAccentColor = false;
    bool showTaskbarFrame = true;
    bool showTaskbarLogo = true;
    TaskbarPillColorMode pillColorMode = TaskbarPillColorMode::Emerald;
    FlyoutDesignStyle flyoutDesign = FlyoutDesignStyle::CompactFluent;
    int flyoutScalePercent = 120;

    AppLanguage language = AppLanguage::Auto;
    std::array<std::wstring, 6> customPrayerNames{};
    std::array<std::wstring, 6> customPrayerShortCodes{};

    // Eski v1 uyumluluk alanları
    bool notifyOnPrayerTime = true;
    int preNotifyMinutes = 15;
    bool playNotificationSound = false;
    bool startWithWindows = false;
    bool autoOptimizeMemory = true;
    bool quietDuringFullscreen = true;
    bool showKerahatIndicator = true;
    bool ramadanIftarSahurMode = true;
    bool fridayReminder = true;
    bool reviewPrompted = false;
    bool notificationsEnabled = true;  // master switch synced with per-prayer rules
    int notifyMuteUntilMin = 0;  // local epoch minutes; notifications stay silent until then
    int fajrSound = -1;          // NotificationSoundKind for Imsak; -1 = same as entrySound
    bool holidayReminders = true;
    bool showExtraTimes = false;
    bool travelMode = true;
    bool prayerTracking = false;
    bool modernToasts = true;
    bool globalHotkey = false;
    int hotkeyMods = 0x0009;     // RegisterHotKey MOD_* flags (default Win+Alt)
    int hotkeyVk = 'P';          // virtual-key code
    int lastTzHash = 0;          // hash of the last seen Windows time zone key; 0 = not recorded yet

    PrayerAlertSchedule alerts{};

    void SetAllNotificationsEnabled(bool enabled) {
        notificationsEnabled = enabled;
        for (auto& r : alerts.rules) {
            r.notifyAtTime = enabled;
            r.playSound = enabled;
        }
    }
    NotificationSoundKind entrySound = NotificationSoundKind::AdhanShort;
    NotificationSoundKind preNotifySound = NotificationSoundKind::GentleChime;
    std::wstring customSoundPath;
    int soundVolumePercent = 80;

    PrayerOffsets offsets{};
};

struct DateYMD {
    int year = 2026;
    int month = 1;
    int day = 1;

    static int64_t DaysFromCivil(int y, int m, int d) {
        y -= m <= 2;
        const int era = (y >= 0 ? y : y - 399) / 400;
        const unsigned yoe = static_cast<unsigned>(y - era * 400);
        const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + static_cast<unsigned>(d) - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
    }

    static DateYMD CivilFromDays(int64_t z) {
        z += 719468;
        const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        const unsigned doe = static_cast<unsigned>(z - era * 146097);
        const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const int64_t y = static_cast<int64_t>(yoe) + era * 400;
        const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        const unsigned mp = (5 * doy + 2) / 153;
        const unsigned d = doy - (153 * mp + 2) / 5 + 1;
        const unsigned m = mp + (mp < 10 ? 3 : -9);
        return {static_cast<int>(y + (m <= 2)), static_cast<int>(m), static_cast<int>(d)};
    }

    int64_t ToEpochDays() const {
        return DaysFromCivil(year, month, day);
    }

    DateYMD AddDays(int delta) const {
        return CivilFromDays(ToEpochDays() + delta);
    }

    int DayOfWeek() const {
        // 1970-01-01 = Perşembe (4) -> 0=Pazar, 1=Pazartesi, ..., 6=Cumartesi
        int64_t d = ToEpochDays() + 4;
        int r = static_cast<int>(d % 7);
        return r < 0 ? r + 7 : r;
    }

    bool operator==(const DateYMD& o) const {
        return year == o.year && month == o.month && day == o.day;
    }
    bool operator<(const DateYMD& o) const {
        if (year != o.year) return year < o.year;
        if (month != o.month) return month < o.month;
        return day < o.day;
    }
};

struct TimeHM {
    int hour = 0;
    int minute = 0;

    int TotalMinutes() const { return hour * 60 + minute; }

    TimeHM AddMinutes(int delta) const {
        int m = std::clamp(TotalMinutes() + delta, 0, 23 * 60 + 59);
        return {m / 60, m % 60};
    }

    std::wstring FormatHHMM() const {
        wchar_t buf[16];
        swprintf_s(buf, L"%02d:%02d", hour, minute);
        return std::wstring(buf);
    }

    bool operator==(const TimeHM& o) const { return hour == o.hour && minute == o.minute; }
    bool operator<(const TimeHM& o) const { return TotalMinutes() < o.TotalMinutes(); }
    bool operator>(const TimeHM& o) const { return TotalMinutes() > o.TotalMinutes(); }
    bool operator<=(const TimeHM& o) const { return TotalMinutes() <= o.TotalMinutes(); }
    bool operator>=(const TimeHM& o) const { return TotalMinutes() >= o.TotalMinutes(); }
};

struct LocalDateTime {
    DateYMD date{};
    int hour = 0;
    int minute = 0;
    int second = 0;

    int64_t ToEpochSeconds() const {
        return date.ToEpochDays() * 86400LL + static_cast<int64_t>(hour) * 3600LL +
               static_cast<int64_t>(minute) * 60LL + static_cast<int64_t>(second);
    }

    static LocalDateTime FromDateTime(DateYMD d, TimeHM t) {
        return {d, t.hour, t.minute, 0};
    }

    bool operator<(const LocalDateTime& o) const { return ToEpochSeconds() < o.ToEpochSeconds(); }
    bool operator>(const LocalDateTime& o) const { return ToEpochSeconds() > o.ToEpochSeconds(); }
};

struct CityLocation {
    std::wstring id;
    std::wstring name;
    std::wstring country;
    std::wstring countryCode;
    double latitude = 0.0;
    double longitude = 0.0;
    std::wstring timeZoneId;
    std::wstring searchAliases;

    std::wstring DisplayLabel() const {
        return name + L", " + country;
    }
};

struct DailyPrayerTimes {
    DateYMD date{};
    TimeHM imsak{};
    TimeHM gunes{};
    TimeHM ogle{};
    TimeHM ikindi{};
    TimeHM aksam{};
    TimeHM yatsi{};
    std::wstring hijriDateText;
    std::wstring source = L"Yerel Hesaplama";

    TimeHM GetTime(PrayerKind kind) const {
        switch (kind) {
            case PrayerKind::Imsak: return imsak;
            case PrayerKind::Gunes: return gunes;
            case PrayerKind::Ogle: return ogle;
            case PrayerKind::Ikindi: return ikindi;
            case PrayerKind::Aksam: return aksam;
            case PrayerKind::Yatsi: return yatsi;
        }
        return imsak;
    }

    std::array<std::pair<PrayerKind, TimeHM>, 6> ToOrderedEntries() const {
        return {{
            {PrayerKind::Imsak, imsak},
            {PrayerKind::Gunes, gunes},
            {PrayerKind::Ogle, ogle},
            {PrayerKind::Ikindi, ikindi},
            {PrayerKind::Aksam, aksam},
            {PrayerKind::Yatsi, yatsi}
        }};
    }
};

struct PrayerCountdownState {
    LocalDateTime calculatedAt{};
    PrayerKind currentPrayer = PrayerKind::Imsak;
    PrayerKind nextPrayer = PrayerKind::Imsak;
    LocalDateTime nextPrayerDateTime{};
    LocalDateTime previousPrayerDateTime{};
    int64_t remainingSeconds = 0;
    double progress = 0.0;
    bool isUrgent = false;
    DailyPrayerTimes today{};
    TimeHM tomorrowImsak{};
    KerahatStatus kerahatStatus = KerahatStatus::None;
};

struct DetectedLocationResult {
    std::wstring cityId;
    std::wstring cityName;
    std::wstring countryName;
    std::wstring countryCode;
    double latitude = 0.0;
    double longitude = 0.0;
    std::wstring timeZoneId;
    CalculationMethod recommendedMethod = CalculationMethod::MWL;
    std::wstring sourceLabel;
};

} // namespace vakit
