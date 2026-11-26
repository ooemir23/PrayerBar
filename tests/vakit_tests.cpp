#include "../src/core/city_catalog.h"
#include "../src/core/formatter.h"
#include "../src/core/i18n.h"
#include "../src/core/license_manager.h"
#include "../src/core/models.h"
#include "../src/platform/win_store.h"
#include "../src/core/prayer_calc.h"
#include "../src/core/schedule_service.h"
#include "../src/core/settings_store.h"
#include "../src/core/utf_util.h"
#include "../src/core/prayer_tracking.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace vakit;

namespace {

int g_passed = 0;

#define CHECK(cond)                                                                                   \
    do {                                                                                              \
        if (!(cond)) {                                                                                \
            fprintf(stderr, "TEST FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
            return 1;                                                                                 \
        }                                                                                             \
    } while (0)

PrayerCountdownState MakeSampleState(PrayerKind next, int remainingMinutes, TimeHM nextTime) {
    DateYMD d{2026, 11, 21};
    DailyPrayerTimes today;
    today.date = d;
    today.imsak = {6, 20};
    today.gunes = {7, 51};
    today.ogle = {12, 55};
    today.ikindi = {15, 25};
    today.aksam = {17, 48};
    today.yatsi = {19, 14};

    LocalDateTime nextDt = LocalDateTime::FromDateTime(d, nextTime);
    LocalDateTime prevDt = LocalDateTime::FromDateTime(d, today.ogle);
    int64_t remSec = static_cast<int64_t>(remainingMinutes) * 60LL;
    return PrayerCountdownState{
        prevDt,
        PrayerKind::Ogle,
        next,
        nextDt,
        prevDt,
        remSec,
        0.75,
        remainingMinutes <= 15,
        today,
        {6, 21}
    };
}

int TestAll23CalculationMethodsAndOrder() {
    DateYMD d{2026, 11, 21};
    for (CalculationMethod m : kOrderedCalculationMethods) {
        DailyPrayerTimes res = CalculatePrayerTimes(d, 41.0082, 28.9784, L"Turkey Standard Time", m);
        CHECK(res.imsak < res.gunes);
        CHECK(res.gunes < res.ogle);
        CHECK(res.ogle < res.ikindi);
        CHECK(res.ikindi < res.aksam);
        CHECK(res.aksam < res.yatsi);
        CHECK(GetMethodFriendlyName(m)[0] != L'\0');
    }
    ++g_passed;
    return 0;
}

int TestIstanbulDiyanetWindowAndHighLatitude() {
    DateYMD nov21{2026, 11, 21};
    DailyPrayerTimes ist = CalculatePrayerTimes(nov21, 41.0082, 28.9784, L"Turkey Standard Time", CalculationMethod::Diyanet);
    CHECK(ist.imsak >= TimeHM({6, 0}) && ist.imsak <= TimeHM({6, 35}));
    CHECK(ist.gunes >= TimeHM({7, 35}) && ist.gunes <= TimeHM({8, 10}));
    CHECK(ist.ogle >= TimeHM({12, 40}) && ist.ogle <= TimeHM({13, 10}));
    CHECK(ist.ikindi >= TimeHM({15, 10}) && ist.ikindi <= TimeHM({15, 45}));
    CHECK(ist.aksam >= TimeHM({17, 30}) && ist.aksam <= TimeHM({18, 5}));
    CHECK(ist.yatsi >= TimeHM({18, 55}) && ist.yatsi <= TimeHM({19, 30}));

    DateYMD jun21{2026, 6, 21};
    DailyPrayerTimes highLat = CalculatePrayerTimes(jun21, 64.0, 10.0, L"W. Europe Standard Time", CalculationMethod::Diyanet);
    CHECK(highLat.imsak < highLat.gunes);
    CHECK(highLat.gunes < highLat.ogle);
    CHECK(highLat.ogle < highLat.ikindi);
    CHECK(highLat.ikindi < highLat.aksam);
    CHECK(highLat.aksam < highLat.yatsi);
    ++g_passed;
    return 0;
}

int TestHanafiAsrAndCountryMapping() {
    DateYMD d{2026, 11, 21};
    auto stdT = CalculatePrayerTimes(d, 41.0082, 28.9784, L"Turkey Standard Time", CalculationMethod::MWL, {}, AsrJuristicMethod::Standard);
    auto hanT = CalculatePrayerTimes(d, 41.0082, 28.9784, L"Turkey Standard Time", CalculationMethod::MWL, {}, AsrJuristicMethod::Hanafi);
    CHECK(hanT.ikindi > stdT.ikindi);
    CHECK(hanT.ogle == stdT.ogle);

    CHECK(GetRecommendedMethodForCountry(L"TR") == CalculationMethod::Diyanet);
    CHECK(GetRecommendedMethodForCountry(L"US") == CalculationMethod::ISNA);
    CHECK(GetRecommendedMethodForCountry(L"SA") == CalculationMethod::UmmAlQura);
    CHECK(GetRecommendedMethodForCountry(L"FR") == CalculationMethod::FranceUOIF);
    CHECK(GetRecommendedMethodForCountry(L"ID") == CalculationMethod::IndonesiaKEMENAG);
    CHECK(GetRecommendedMethodForCountry(L"GB") == CalculationMethod::MWL);
    ++g_passed;
    return 0;
}

int TestLocationAwareScheduleCacheKeys() {
    AppSettings adelaide;
    adelaide.cityId = L"custom";
    adelaide.latitude = -34.9285;
    adelaide.longitude = 138.6007;
    adelaide.timeZoneId = L"Cen. Australia Standard Time";

    AppSettings sydney = adelaide;
    sydney.latitude = -33.8688;
    sydney.longitude = 151.2093;
    sydney.timeZoneId = L"AUS Eastern Standard Time";
    CHECK(ScheduleService::BuildMonthKey(2026, 11, adelaide) !=
          ScheduleService::BuildMonthKey(2026, 11, sydney));

    AppSettings alternateZone = adelaide;
    alternateZone.timeZoneId = L"AUS Eastern Standard Time";
    CHECK(ScheduleService::BuildMonthKey(2026, 11, adelaide) !=
          ScheduleService::BuildMonthKey(2026, 11, alternateZone));
    ++g_passed;
    return 0;
}

int TestDisplayFormatterStylesAndTooltip() {
    auto state = MakeSampleState(PrayerKind::Ikindi, 23, {15, 25});
    AppSettings settings;
    settings.language = AppLanguage::Turkish;

    settings.displayStyle = TaskbarDisplayStyle::TwoLineClassic;
    auto f1 = FormatTaskbar(state, settings);
    CHECK(f1.line1 == L"İkindiye kalan");
    CHECK(f1.hasLine2 && f1.line2 == L"23 dk");

    settings.displayStyle = TaskbarDisplayStyle::CompactArrow;
    auto f2 = FormatTaskbar(state, settings);
    CHECK(f2.line1 == L"İ→23Dk");
    CHECK(!f2.hasLine2);

    settings.displayStyle = TaskbarDisplayStyle::Custom;
    settings.customTemplate = L"{vakit} ({saat})\\n{kisa}→{kalan_kisa}";
    auto f3 = FormatTaskbar(state, settings);
    CHECK(f3.line1 == L"İkindi (15:25)");
    CHECK(f3.hasLine2 && f3.line2 == L"İ→23Dk");

    for (int i = 0; i <= 8; ++i) {
        settings.displayStyle = static_cast<TaskbarDisplayStyle>(i);
        auto fx = FormatTaskbar(state, settings);
        CHECK(!fx.line1.empty());
    }

    settings.cityName = std::wstring(100, L'A');
    settings.showSeconds = true;
    CHECK(FormatTrayTooltip(state, settings).size() <= 127);

    // FormatTrayText rounding and compact badges
    {
        auto s0 = MakeSampleState(PrayerKind::Imsak, 0, {6, 0});
        s0.remainingSeconds = 0;
        CHECK(FormatTrayText(s0, settings).second == L"<1");

        auto s23 = MakeSampleState(PrayerKind::Ikindi, 23, {15, 25});
        CHECK(FormatTrayText(s23, settings).second == L"23");

        auto s105 = MakeSampleState(PrayerKind::Imsak, 105, {6, 0}); // 1h 45m -> 2s
        CHECK(FormatTrayText(s105, settings).second == L"2s");

        auto s430 = MakeSampleState(PrayerKind::Imsak, 430, {6, 0}); // 7h 10m -> 7s
        CHECK(FormatTrayText(s430, settings).second == L"7s");

        // trayShowMinutesUpTo99
        AppSettings s99 = settings;
        s99.trayShowMinutesUpTo99 = true;
        auto s85 = MakeSampleState(PrayerKind::Imsak, 85, {6, 0});
        CHECK(FormatTrayText(s85, s99).second == L"85");
        auto s99m = MakeSampleState(PrayerKind::Imsak, 99, {6, 0});
        CHECK(FormatTrayText(s99m, s99).second == L"99");
        CHECK(FormatTrayText(s105, s99).second == L"2s");
    }
    ++g_passed;
    return 0;
}

int TestMidnightRolloverAndQiblaAndCatalog() {
    wchar_t tmpRoot[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpRoot);
    std::wstring tempDir = std::wstring(tmpRoot) + L"VakitCppTests_" + std::to_wstring(GetCurrentProcessId());
    {
        ScheduleService svc(tempDir + L"\\cache");
        AppSettings s;
        s.useOnlineSync = false;

        LocalDateTime early{{2026, 11, 21}, 3, 30, 0};
        auto stEarly = svc.ComputeState(early, s);
        CHECK(stEarly.currentPrayer == PrayerKind::Yatsi);
        CHECK(stEarly.nextPrayer == PrayerKind::Imsak);
        CHECK(stEarly.remainingSeconds > 0);
        CHECK(stEarly.progress >= 0.0 && stEarly.progress <= 1.0);

        LocalDateTime late{{2026, 11, 21}, 23, 30, 0};
        auto stLate = svc.ComputeState(late, s);
        CHECK(stLate.currentPrayer == PrayerKind::Yatsi);
        CHECK(stLate.nextPrayer == PrayerKind::Imsak);
        CHECK(stLate.nextPrayerDateTime.date == DateYMD({2026, 11, 22}));
        CHECK(stLate.remainingSeconds > 0);
        CHECK(stLate.progress >= 0.0 && stLate.progress <= 1.0);

        // Settings JSON round-trip (per-prayer rules + FarRight + AdhanMakkah + Hanafi)
        SettingsStore store(tempDir);
        s.dockPosition = TaskbarDockPosition::FarRight;
        s.asrMethod = AsrJuristicMethod::Hanafi;
        s.entrySound = NotificationSoundKind::AdhanMakkah;
        s.preNotifySound = NotificationSoundKind::WarmBell;
        s.soundVolumePercent = 65;
        s.cityId = L"custom";
        s.latitude = 80.0;
        s.longitude = -179.0;
        s.useCustomCoordinates = true;
        s.autoDetectLocation = false;
        s.alerts.GetRule(PrayerKind::Imsak).preNotifyMinutes = 45;
        s.alerts.GetRule(PrayerKind::Ikindi).preNotifyMinutes = 10;
        s.alerts.GetRule(PrayerKind::Gunes).playSound = false;
        s.alerts.GetRule(PrayerKind::Yatsi).notifyAtTime = false;
        CHECK(store.Save(s));

        AppSettings loaded = store.Load();
        CHECK(loaded.dockPosition == TaskbarDockPosition::FarRight);
        CHECK(loaded.asrMethod == AsrJuristicMethod::Hanafi);
        CHECK(loaded.entrySound == NotificationSoundKind::AdhanMakkah);
        CHECK(loaded.preNotifySound == NotificationSoundKind::WarmBell);
        CHECK(loaded.soundVolumePercent == 65);
        CHECK(loaded.latitude == 80.0);
        CHECK(loaded.longitude == -179.0);
        CHECK(!loaded.autoDetectLocation);
        CHECK(loaded.alerts.GetRule(PrayerKind::Imsak).preNotifyMinutes == 45);
        CHECK(loaded.alerts.GetRule(PrayerKind::Ikindi).preNotifyMinutes == 10);
        CHECK(!loaded.alerts.GetRule(PrayerKind::Gunes).playSound);
        CHECK(!loaded.alerts.GetRule(PrayerKind::Yatsi).notifyAtTime);

        std::wstring legacyPath = tempDir + L"\\legacy-settings.json";
        CHECK(WriteFileAtomicallyUtf8(
            legacyPath,
            R"({"cityId":"sydney","cityName":"Sydney","latitude":-33.8688,"longitude":151.2093,"useCustomCoordinates":false})"));
        auto legacySettings = store.ImportFromFile(legacyPath);
        CHECK(legacySettings.has_value());
        CHECK(!legacySettings->autoDetectLocation);

        std::wstring defaultPath = tempDir + L"\\default-settings.json";
        CHECK(WriteFileAtomicallyUtf8(defaultPath, R"({"cityId":"istanbul"})"));
        auto defaultSettings = store.ImportFromFile(defaultPath);
        CHECK(defaultSettings.has_value());
        CHECK(defaultSettings->autoDetectLocation);
    }
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);

    // City Catalog & Qibla & Hijri
    int trCount = 0;
    for (const auto& c : GetAllCities()) {
        if (c.country == L"Türkiye") ++trCount;
    }
    CHECK(trCount >= 81);

    auto izmir = SearchCities(L"izmir", 20);
    CHECK(!izmir.empty() && izmir[0].id == L"izmir");
    auto urfa = SearchCities(L"sanliurfa", 20);
    CHECK(!urfa.empty() && urfa[0].id == L"sanliurfa");
    auto adelaide = SearchCities(L"adelaide", 20);
    CHECK(!adelaide.empty() && adelaide[0].id == L"adelaide");
    CHECK(adelaide[0].countryCode == L"AU");
    CHECK(adelaide[0].timeZoneId == L"Cen. Australia Standard Time");
    CHECK(std::abs(GetTimeZoneOffsetHours({2026, 1, 1}, adelaide[0].timeZoneId) - 10.5) < 0.1);
    CHECK(std::abs(GetTimeZoneOffsetHours({2026, 7, 1}, adelaide[0].timeZoneId) - 9.5) < 0.1);

    auto nearIst = FindNearestCity(41.009, 28.979, 30.0);
    CHECK(nearIst.has_value() && nearIst->id == L"istanbul");
    CHECK(!FindNearestCity(0.0, 0.0, 30.0).has_value());

    double bearing = CalculateQiblaBearing(41.0082, 28.9784);
    CHECK(bearing >= 148.0 && bearing <= 155.0);
    CHECK(std::wstring(GetTurkishCompassDirection(bearing)) == L"GD");
    CHECK(!FormatTurkishHijriDate({2026, 11, 21}).empty());

    ++g_passed;
    return 0;
}

int TestMultiLanguageHijriAndRamadan() {
    AppLanguage langs[12] = {
        AppLanguage::Turkish, AppLanguage::English, AppLanguage::Arabic,
        AppLanguage::German, AppLanguage::French, AppLanguage::Indonesian,
        AppLanguage::Urdu, AppLanguage::Bengali, AppLanguage::Malay,
        AppLanguage::Persian, AppLanguage::Russian, AppLanguage::Spanish
    };

    for (AppLanguage lang : langs) {
        CHECK(Tr(lang, StrId::SettingsWindowTitle)[0] != L'\0');
        CHECK(Tr(lang, StrId::OpenImsakiye)[0] != L'\0');
        CHECK(GetDefaultPrayerName(PrayerKind::Ikindi, lang)[0] != L'\0');
        CHECK(GetDefaultPrayerShortCode(PrayerKind::Ikindi, lang)[0] != L'\0');
        CHECK(!FormatHijriDateLocalized({2026, 11, 21}, lang).empty());
        CHECK(!FormatQiblaLabelLocalized(41.0082, 28.9784, lang).empty());
    }

    AppLanguage autoLang = ResolveEffectiveLanguage(AppLanguage::Auto);
    CHECK(static_cast<int>(autoLang) >= 1 && static_cast<int>(autoLang) <= 12);

    // New languages must be fully translated (no silent English fallback), except brand-only rows
    for (int li = 7; li <= 12; ++li) {
        AppLanguage lang = static_cast<AppLanguage>(li);
        int sameAsEnglish = 0;
        for (int sid = 0; sid <= static_cast<int>(StrId::ChkTaskbarLogo); ++sid) {
            if (std::wstring(Tr(lang, static_cast<StrId>(sid))) == Tr(AppLanguage::English, static_cast<StrId>(sid))) ++sameAsEnglish;
        }
        CHECK(sameAsEnglish <= 16);
    }
    CHECK(IsRtlLanguage(AppLanguage::Urdu) && IsRtlLanguage(AppLanguage::Persian) && !IsRtlLanguage(AppLanguage::Russian));
    // RTL countdowns are wrapped in an RTL embedding so digits and units keep their order; LTR ones are untouched
    std::wstring urCount = FormatRemainingLong(23 * 60, true, AppLanguage::Urdu);
    CHECK(urCount.front() == L'\u202B' && urCount.back() == L'\u202C');
    CHECK(FormatRemainingCompact(90, false, AppLanguage::Persian).front() == L'\u202B');
    CHECK(FormatRemainingLong(23 * 60, true, AppLanguage::English).front() == L'2');

    DateYMD ramStart = FindNextOrCurrentRamadanStart({2026, 11, 21});
    HijriDateYMD hStart = GregorianToHijri(ramStart);
    CHECK(hStart.month == 9 && hStart.day == 1);

    // Holiday eves: the day before 1 Ramadan announces Ramadan; 27 Ramadan (Qadr) is a night observed the evening before
    HolidayEve ramEve = FindHolidayStartingTomorrow(ramStart.AddDays(-1));
    CHECK(ramEve.id == HolidayId::RamadanStart && !ramEve.isNight);
    HolidayEve qadrEve = FindHolidayStartingTomorrow(ramStart.AddDays(25));
    CHECK(qadrEve.id == HolidayId::Qadr && qadrEve.isNight);
    CHECK(FindHolidayStartingTomorrow(ramStart.AddDays(3)).id == HolidayId::None);

    // Diyanet 2026 table: Ramadan starts 19 Feb, Eid al-Fitr 20-22 Mar (arife 19 Mar), Kurban 27-30 May, Regaib night 10 Dec
    CHECK(FindHolidayStartingTomorrow({2026, 2, 18}).id == HolidayId::RamadanStart);
    CHECK(FindHolidayStartingTomorrow({2026, 3, 19}).id == HolidayId::EidFitr);
    CHECK(FindHolidayStartingTomorrow({2026, 5, 26}).id == HolidayId::EidAdha);
    HolidayEve regaib = FindHolidayStartingTomorrow({2026, 12, 10});
    CHECK(regaib.id == HolidayId::Raghaib && regaib.isNight);
    CHECK(FindNextOrCurrentRamadanStart({2026, 1, 5}) == DateYMD({2026, 2, 19}));
    int eidDays = 0;
    for (const auto& r : ListReligiousDays(2026)) if (r.id == HolidayId::EidAdha) eidDays = r.span;
    CHECK(eidDays == 4);
    CHECK(ListReligiousDays(2027).size() >= 10);  // tabular fallback for years without a Diyanet table

    // Extra times: Maghrib 18:00, next Fajr 06:00 -> 12h night, midnight 00:00, last third 02:00; Duha = sunrise + 45 min
    {
        DailyPrayerTimes d{};
        d.gunes = {7, 0};
        d.aksam = {18, 0};
        ExtraTimes x = ComputeExtraTimes(d, {6, 0});
        CHECK(x.midnight.hour == 0 && x.midnight.minute == 0);
        CHECK(x.tahajjud.hour == 2 && x.tahajjud.minute == 0);
        CHECK(x.duha.hour == 7 && x.duha.minute == 45);
    }

    // Prayer tracking persists to disk and counts only farz prayers
    {
        wchar_t tmp[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tmp);
        std::wstring dir = std::wstring(tmp) + L"VakitTrackTest_" + std::to_wstring(GetCurrentProcessId());
        std::filesystem::create_directories(dir);
        tracking::SetStorageDirForTesting(dir);
        DateYMD day{2026, 11, 20};
        tracking::TogglePrayed(day, PrayerKind::Ogle);
        tracking::TogglePrayed(day, PrayerKind::Gunes);  // ignored
        tracking::TogglePrayed(day.AddDays(1), PrayerKind::Yatsi);
        tracking::SetStorageDirForTesting(dir);  // forces a reload from the file
        CHECK(tracking::IsPrayed(day, PrayerKind::Ogle));
        CHECK(!tracking::IsPrayed(day, PrayerKind::Gunes));
        CHECK(tracking::CountPrayedInLastDays(day.AddDays(1), 7) == 2);
        tracking::TogglePrayed(day, PrayerKind::Ogle);
        CHECK(!tracking::IsPrayed(day, PrayerKind::Ogle));
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    // Ramazan içindeyken de o Ramazanın 1. gününü bulmalı
    DateYMD midRam = ramStart.AddDays(12);
    CHECK(FindNextOrCurrentRamadanStart(midRam) == ramStart);

    ++g_passed;
    return 0;
}

int TestCustomPrayerNamesSettingsStoreExportAndLockout() {
    auto state = MakeSampleState(PrayerKind::Ikindi, 23, {15, 25});
    AppSettings s;
    s.language = AppLanguage::English;
    s.flyoutDesign = FlyoutDesignStyle::TimelineDashboard;
    s.pillColorMode = TaskbarPillColorMode::ThemeAdaptive;
    s.flyoutScalePercent = 145;
    s.useSystemAccentColor = false;
    s.showTaskbarFrame = false;
    s.showTaskbarLogo = false;
    s.autoOptimizeMemory = false;
    s.quietDuringFullscreen = false;
    s.showKerahatIndicator = false;
    s.customPrayerNames[static_cast<size_t>(PrayerKind::Ikindi)] = L"Afternoon";
    s.customPrayerShortCodes[static_cast<size_t>(PrayerKind::Ikindi)] = L"Af";
    s.displayStyle = TaskbarDisplayStyle::CompactArrow;

    auto disp = FormatTaskbar(state, s);
    CHECK(disp.line1 == L"Af→23m");

    s.displayStyle = TaskbarDisplayStyle::TwoLineClassic;
    auto disp2 = FormatTaskbar(state, s);
    CHECK(disp2.line1 == L"Until Afternoon");

    wchar_t tmpRoot[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpRoot);
    std::wstring tempDir = std::wstring(tmpRoot) + L"VakitCppTests2_" + std::to_wstring(GetCurrentProcessId());
    {
        SettingsStore store(tempDir);
        CHECK(store.Save(s));
        AppSettings loaded = store.Load();
        CHECK(loaded.language == AppLanguage::English);
        CHECK(loaded.flyoutDesign == FlyoutDesignStyle::TimelineDashboard);
        CHECK(loaded.pillColorMode == TaskbarPillColorMode::ThemeAdaptive);
        CHECK(loaded.flyoutScalePercent == 145);
        CHECK(!loaded.useSystemAccentColor);
        CHECK(!loaded.showTaskbarFrame);
        CHECK(!loaded.showTaskbarLogo);
        CHECK(!loaded.autoOptimizeMemory);
        CHECK(!loaded.quietDuringFullscreen);
        CHECK(!loaded.showKerahatIndicator);
        CHECK(loaded.customPrayerNames[static_cast<size_t>(PrayerKind::Ikindi)] == L"Afternoon");
        CHECK(loaded.customPrayerShortCodes[static_cast<size_t>(PrayerKind::Ikindi)] == L"Af");

        // Export & Import test
        std::wstring exportPath = tempDir + L"\\backup.json";
        CHECK(store.ExportToFile(loaded, exportPath));
        auto imported = store.ImportFromFile(exportPath);
        CHECK(imported.has_value());
        CHECK(imported->customPrayerNames[static_cast<size_t>(PrayerKind::Ikindi)] == L"Afternoon");

        // Lock-out guard: hem pill hem tepsi kapalıysa tepsi otomatik açılmalı
        AppSettings locked = s;
        locked.showTaskbarPill = false;
        locked.showTrayIcon = false;
        CHECK(store.Save(locked));
        AppSettings unlocked = store.Load();
        CHECK(unlocked.showTrayIcon == true);

        // LargeDigits + trayShowMinutesUpTo99 tray style persistence
        AppSettings trayLg = s;
        trayLg.trayStyle = TrayIconStyle::LargeDigits;
        trayLg.trayShowMinutesUpTo99 = true;
        CHECK(store.Save(trayLg));
        AppSettings trayLgLoaded = store.Load();
        CHECK(trayLgLoaded.trayStyle == TrayIconStyle::LargeDigits);
        CHECK(trayLgLoaded.trayShowMinutesUpTo99 == true);

        // Master notification switch syncs per-prayer toggles while keeping preNotifyMinutes
        AppSettings notif = s;
        for (int i = 0; i < 6; ++i) {
            notif.alerts.GetRule(static_cast<PrayerKind>(i)).preNotifyMinutes = 60;
        }
        notif.SetAllNotificationsEnabled(false);
        CHECK(!notif.notificationsEnabled);
        for (int i = 0; i < 6; ++i) {
            const auto& r = notif.alerts.GetRule(static_cast<PrayerKind>(i));
            CHECK(!r.notifyAtTime);
            CHECK(!r.playSound);
            CHECK(r.preNotifyMinutes == 60);
        }
        CHECK(store.Save(notif));
        AppSettings notifReloaded = store.Load();
        CHECK(!notifReloaded.notificationsEnabled);
        for (int i = 0; i < 6; ++i) {
            const auto& r = notifReloaded.alerts.GetRule(static_cast<PrayerKind>(i));
            CHECK(!r.notifyAtTime);
            CHECK(!r.playSound);
            CHECK(r.preNotifyMinutes == 60);
        }
        notifReloaded.SetAllNotificationsEnabled(true);
        CHECK(notifReloaded.notificationsEnabled);
        for (int i = 0; i < 6; ++i) {
            const auto& r = notifReloaded.alerts.GetRule(static_cast<PrayerKind>(i));
            CHECK(r.notifyAtTime);
            CHECK(r.playSound);
            CHECK(r.preNotifyMinutes == 60);
        }
    }
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);

    ++g_passed;
    return 0;
}

int TestMonthlyImsakiyeAndKerahat() {
    wchar_t tmpRoot[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpRoot);
    std::wstring tempDir = std::wstring(tmpRoot) + L"VakitCppTests3_" + std::to_wstring(GetCurrentProcessId());
    {
        ScheduleService svc(tempDir + L"\\cache");
        AppSettings s;
        s.useOnlineSync = false;

        auto feb26 = svc.GetMonthSchedule(2026, 2, s, false);
        CHECK(feb26.size() == 28);
        auto feb24 = svc.GetMonthSchedule(2024, 2, s, false);
        CHECK(feb24.size() == 29);
        auto nov26 = svc.GetMonthSchedule(2026, 11, s, false);
        CHECK(nov26.size() == 30);

        for (const auto& d : nov26) {
            CHECK(d.imsak < d.gunes);
            CHECK(d.gunes < d.ogle);
            CHECK(d.ogle < d.ikindi);
            CHECK(d.ikindi < d.aksam);
            CHECK(d.aksam < d.yatsi);
        }

        const auto& day = nov26[20]; // Nov 21
        CHECK(ComputeKerahatStatus(LocalDateTime::FromDateTime(day.date, day.gunes.AddMinutes(10)), day) == KerahatStatus::AfterSunrise);
        CHECK(ComputeKerahatStatus(LocalDateTime::FromDateTime(day.date, day.ogle.AddMinutes(-15)), day) == KerahatStatus::BeforeDhuhr);
        CHECK(ComputeKerahatStatus(LocalDateTime::FromDateTime(day.date, day.aksam.AddMinutes(-15)), day) == KerahatStatus::BeforeMaghrib);
        CHECK(ComputeKerahatStatus(LocalDateTime::FromDateTime(day.date, day.ogle.AddMinutes(20)), day) == KerahatStatus::None);
    }
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);

    ++g_passed;
    return 0;
}

int TestLicenseManagerTrialOfflineKeysAndGating() {
    wchar_t tmpRoot[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpRoot);
    std::wstring tempDir = std::wstring(tmpRoot) + L"VakitCppLicTests_" + std::to_wstring(GetCurrentProcessId());
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);

    {
        LicenseManager lm(tempDir, false);
        DateYMD day0 = LicenseManager::CurrentLocalDate();  // ctor stamps the real date

        auto st0 = lm.GetStatus(day0);
        CHECK(st0.tier == LicenseTier::Trial);
        CHECK(st0.trialDaysRemaining == 5);
        CHECK(st0.IsPro());

        auto st4 = lm.GetStatus(day0.AddDays(4));
        CHECK(st4.tier == LicenseTier::Trial);
        CHECK(st4.trialDaysRemaining == 1);
        CHECK(st4.IsPro());

        auto st5 = lm.GetStatus(day0.AddDays(5));
        CHECK(st5.tier == LicenseTier::Free);
        CHECK(st5.trialDaysRemaining == 0);
        CHECK(!st5.IsPro());
        CHECK(!lm.IsProActive(day0.AddDays(5)));

        // Ethical Pro gating rules
        for (int i = 0; i <= 3; ++i) {
            CHECK(!LicenseManager::IsFeatureProOnly(static_cast<TaskbarDisplayStyle>(i)));
        }
        for (int i = 4; i <= 8; ++i) {
            CHECK(LicenseManager::IsFeatureProOnly(static_cast<TaskbarDisplayStyle>(i)));
        }
        CHECK(!LicenseManager::IsFeatureProOnly(FlyoutDesignStyle::CompactFluent));
        CHECK(LicenseManager::IsFeatureProOnly(FlyoutDesignStyle::MinimalGlass));
        CHECK(LicenseManager::IsFeatureProOnly(FlyoutDesignStyle::CardGrid));
        CHECK(LicenseManager::IsFeatureProOnly(FlyoutDesignStyle::TimelineDashboard));
    }

    std::filesystem::remove_all(tempDir, ec);
    ++g_passed;
    return 0;
}

int TestRamadanIftarSahurModeAndGlobalAutoLang() {
    AppSettings def;
    CHECK(def.language == AppLanguage::Auto);
    CHECK(def.ramadanIftarSahurMode == true);

    wchar_t tmpRoot[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpRoot);
    std::wstring tempDir = std::wstring(tmpRoot) + L"VakitCppRamTests_" + std::to_wstring(GetCurrentProcessId());
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    {
        SettingsStore store(tempDir);
        AppSettings s;
        s.ramadanIftarSahurMode = false;
        CHECK(store.Save(s));
        AppSettings loaded = store.Load();
        CHECK(loaded.ramadanIftarSahurMode == false);
    }
    std::filesystem::remove_all(tempDir, ec);

    DateYMD ramDay = FindNextOrCurrentRamadanStart({2026, 11, 21}).AddDays(5);
    CHECK(GregorianToHijri(ramDay).month == 9);

    PrayerCountdownState st;
    st.calculatedAt = {ramDay, 18, 22, 0};
    st.currentPrayer = PrayerKind::Ikindi;
    st.nextPrayer = PrayerKind::Aksam;
    st.nextPrayerDateTime = {ramDay, 18, 45, 0};
    st.remainingSeconds = 23 * 60;

    AppSettings cfg;
    cfg.displayStyle = TaskbarDisplayStyle::TwoLineClassic;
    cfg.ramadanIftarSahurMode = true;
    cfg.language = AppLanguage::Turkish;
    CHECK(FormatTaskbar(st, cfg).line1 == L"İftara kalan");

    cfg.language = AppLanguage::English;
    CHECK(FormatTaskbar(st, cfg).line1 == L"Until Iftar");

    cfg.ramadanIftarSahurMode = false;
    cfg.language = AppLanguage::Turkish;
    CHECK(FormatTaskbar(st, cfg).line1 == L"Akşama kalan");

    st.nextPrayer = PrayerKind::Imsak;
    st.nextPrayerDateTime = {ramDay, 5, 10, 0};
    cfg.ramadanIftarSahurMode = true;
    cfg.language = AppLanguage::Turkish;
    CHECK(FormatTaskbar(st, cfg).line1 == L"Sahura kalan");

    cfg.language = AppLanguage::English;
    CHECK(FormatTaskbar(st, cfg).line1 == L"Until Suhoor");

    ++g_passed;
    return 0;
}

int TestMicrosoftStoreIapHelpersAndLicenseSync() {
    // 1. WinRT DateTime (100-ns ticks since 1601-01-01) conversion
    SYSTEMTIME stUtc{2027, 11, 0, 21, 12, 0, 0, 0};
    FILETIME ft{};
    CHECK(SystemTimeToFileTime(&stUtc, &ft));
    int64_t ticks = (static_cast<int64_t>(ft.dwHighDateTime) << 32) | static_cast<int64_t>(ft.dwLowDateTime);
    DateYMD parsed{};
    CHECK(ConvertWinRtDateTimeToYmd(ticks, parsed));
    CHECK(parsed.year == 2027 && parsed.month == 11 && parsed.day == 21);

    DateYMD farFuture{};
    CHECK(ConvertWinRtDateTimeToYmd(2650467743990000000LL, farFuture));
    CHECK(farFuture.year == 9999 && farFuture.month == 12 && farFuture.day == 31);

    DateYMD bad{};
    CHECK(!ConvertWinRtDateTimeToYmd(0, bad));
    CHECK(!ConvertWinRtDateTimeToYmd(-100, bad));

    // 2. Store Add-On SKU ID & InAppOfferToken classification
    CHECK(ClassifyStoreAddOn(L"9NANNUAL1234/0010", L"", L"9NANNUAL1234", L"9NLIFE9999") == LicenseTier::ProAnnual);
    CHECK(ClassifyStoreAddOn(L"9NLIFE9999/0010", L"", L"9NANNUAL1234", L"9NLIFE9999") == LicenseTier::ProLifetime);
    CHECK(ClassifyStoreAddOn(L"9NUNKNOWN/0010", L"pro_annual", L"", L"") == LicenseTier::ProAnnual);
    CHECK(ClassifyStoreAddOn(L"9NUNKNOWN/0010", L"PRO_LIFETIME", L"", L"") == LicenseTier::ProLifetime);
    CHECK(ClassifyStoreAddOn(L"9NOTHER/0010", L"other_addon", L"9NANNUAL1234", L"9NLIFE9999") == LicenseTier::Free);

    // 3. ApplyStoreLicenseSnapshot in-memory trust, tamper resistance & refund revocation
    wchar_t tmpRoot[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpRoot);
    std::wstring tempDir = std::wstring(tmpRoot) + L"VakitCppStoreTests_" + std::to_wstring(GetCurrentProcessId());
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    {
        DateYMD day0{2026, 11, 21};
        LicenseManager lmReloaded(tempDir, false);
        StoreLicenseSnapshot lifeSnap;
        lifeSnap.storeAvailable = true;
        lifeSnap.appLicenseActive = true;
        lifeSnap.hasProLifetime = true;
        CHECK(lmReloaded.ApplyStoreLicenseSnapshot(lifeSnap, day0));
        auto st = lmReloaded.GetStatus(day0.AddDays(30));
        CHECK(st.tier == LicenseTier::ProLifetime);
        CHECK(st.maskedKey == L"Microsoft Store");
        CHECK(st.IsPro());

        // Pro is never persisted: a fresh instance (and a hand-edited license.json) stays Free after trial
        {
            std::string forged = "{\"firstLaunchEpochDays\": " + std::to_string(day0.ToEpochDays()) +
                                 ", \"tier\": 3, \"fromMicrosoftStore\": true, \"storedKey\": \"X\"}";
            CHECK(WriteFileAtomicallyUtf8(tempDir + L"\\license.json", forged));
            LicenseManager lmForged(tempDir, false);
            CHECK(lmForged.GetStatus(day0.AddDays(30)).tier == LicenseTier::Free);
        }

        // Offline / unavailable Store must NOT revoke cached license
        StoreLicenseSnapshot offlineSnap;
        offlineSnap.storeAvailable = false;
        CHECK(!lmReloaded.ApplyStoreLicenseSnapshot(offlineSnap, day0.AddDays(30)));
        CHECK(lmReloaded.GetStatus(day0.AddDays(30)).tier == LicenseTier::ProLifetime);

        // Store confirms add-on refunded / removed -> revokes Pro
        StoreLicenseSnapshot revokedSnap;
        revokedSnap.storeAvailable = true;
        revokedSnap.appLicenseActive = true;
        CHECK(lmReloaded.ApplyStoreLicenseSnapshot(revokedSnap, day0.AddDays(30)));
        auto stRevoked = lmReloaded.GetStatus(day0.AddDays(30));
        CHECK(stRevoked.tier == LicenseTier::Free);
        CHECK(!stRevoked.IsPro());

        // Annual subscription snapshot + expiration boundary
        StoreLicenseSnapshot annSnap;
        annSnap.storeAvailable = true;
        annSnap.hasProAnnual = true;
        annSnap.annualExpiryIso = L"2027-11-21";
        CHECK(lmReloaded.ApplyStoreLicenseSnapshot(annSnap, day0));
        CHECK(lmReloaded.GetStatus({2027, 11, 21}).tier == LicenseTier::ProAnnual);
        CHECK(lmReloaded.GetStatus({2027, 11, 22}).tier == LicenseTier::Free);
    }
    std::filesystem::remove_all(tempDir, ec);

    // 4. Unpackaged runtime safety check
    StoreLicenseSnapshot liveUnpackaged = QueryMicrosoftStoreLicenseSync();
    CHECK(!liveUnpackaged.storeAvailable);

    ++g_passed;
    return 0;
}

} // namespace

int main() {
    if (TestAll23CalculationMethodsAndOrder() != 0) return 1;
    if (TestIstanbulDiyanetWindowAndHighLatitude() != 0) return 1;
    if (TestHanafiAsrAndCountryMapping() != 0) return 1;
    if (TestLocationAwareScheduleCacheKeys() != 0) return 1;
    if (TestDisplayFormatterStylesAndTooltip() != 0) return 1;
    if (TestMidnightRolloverAndQiblaAndCatalog() != 0) return 1;
    if (TestMultiLanguageHijriAndRamadan() != 0) return 1;
    if (TestCustomPrayerNamesSettingsStoreExportAndLockout() != 0) return 1;
    if (TestMonthlyImsakiyeAndKerahat() != 0) return 1;
    if (TestLicenseManagerTrialOfflineKeysAndGating() != 0) return 1;
    if (TestRamadanIftarSahurModeAndGlobalAutoLang() != 0) return 1;
    if (TestMicrosoftStoreIapHelpersAndLicenseSync() != 0) return 1;
    printf("All %d native C++20 unit test suites passed!\n", g_passed);
    return 0;
}
