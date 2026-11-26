#include "prayer_calc.h"
#include "utf_util.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <cstdio>

namespace vakit {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;

double SinD(double d) { return std::sin(d * kDegToRad); }
double CosD(double d) { return std::cos(d * kDegToRad); }
double TanD(double d) { return std::tan(d * kDegToRad); }
double AsinD(double x) { return std::asin(std::clamp(x, -1.0, 1.0)) * kRadToDeg; }
double AcosD(double x) { return std::acos(std::clamp(x, -1.0, 1.0)) * kRadToDeg; }
double AtanD(double x) { return std::atan(x) * kRadToDeg; }
double Atan2D(double y, double x) { return std::atan2(y, x) * kRadToDeg; }

double FixAngle(double deg) {
    deg = std::fmod(deg, 360.0);
    return deg < 0.0 ? deg + 360.0 : deg;
}

double FixHour(double hour) {
    hour = std::fmod(hour, 24.0);
    return hour < 0.0 ? hour + 24.0 : hour;
}

double ToJulianDate(int year, int month, int day) {
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    int a = year / 100;
    int b = 2 - a + a / 4;
    return std::floor(365.25 * (year + 4716)) +
           std::floor(30.6001 * (month + 1)) +
           day + b - 1524.5;
}

std::pair<double, double> SunDeclinationAndEqT(double jd) {
    double d = jd - 2451545.0;
    double g = FixAngle(357.529 + 0.98560028 * d);
    double q = FixAngle(280.459 + 0.98564736 * d);
    double l = FixAngle(q + 1.915 * SinD(g) + 0.020 * SinD(2.0 * g));
    double e = 23.439 - 0.00000036 * d;
    double raHours = Atan2D(CosD(e) * SinD(l), CosD(l)) / 15.0;
    double eqt = q / 15.0 - FixHour(raHours);
    double decl = AsinD(SinD(e) * SinD(l));
    return {decl, eqt};
}

double SolarNoonCore(double jd, double longitude, double tzHours) {
    auto [decl, eqt] = SunDeclinationAndEqT(jd);
    (void)decl;
    return 12.0 + tzHours - longitude / 15.0 - eqt;
}

double ComputeSolarNoon(double julianBase, double dayFraction, double longitude, double tzHours) {
    double t = SolarNoonCore(julianBase + dayFraction, longitude, tzHours);
    return SolarNoonCore(julianBase + t / 24.0, longitude, tzHours);
}

double SunAngleCore(double jd, double latitude, double longitude, double tzHours, double depressionDeg, bool isMorning) {
    auto [decl, eqt] = SunDeclinationAndEqT(jd);
    double noon = 12.0 + tzHours - longitude / 15.0 - eqt;
    double cosH = (-SinD(depressionDeg) - SinD(latitude) * SinD(decl)) / (CosD(latitude) * CosD(decl));
    if (cosH < -0.9999 || cosH > 0.9999) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    double hourAngle = AcosD(cosH) / 15.0;
    return isMorning ? (noon - hourAngle) : (noon + hourAngle);
}

double ComputeSunAngleTime(double julianBase, double dayFraction, double latitude, double longitude, double tzHours, double depressionDeg, bool isMorning) {
    double t = SunAngleCore(julianBase + dayFraction, latitude, longitude, tzHours, depressionDeg, isMorning);
    if (std::isnan(t)) return t;
    return SunAngleCore(julianBase + t / 24.0, latitude, longitude, tzHours, depressionDeg, isMorning);
}

double AsrCore(double jd, double latitude, double longitude, double tzHours, double shadowFactor) {
    auto [decl, eqt] = SunDeclinationAndEqT(jd);
    double noon = 12.0 + tzHours - longitude / 15.0 - eqt;
    double altitude = AtanD(1.0 / (shadowFactor + TanD(std::abs(latitude - decl))));
    double cosH = (SinD(altitude) - SinD(latitude) * SinD(decl)) / (CosD(latitude) * CosD(decl));
    if (cosH < -0.9999 || cosH > 0.9999) {
        return noon + 3.0;
    }
    double hourAngle = AcosD(cosH) / 15.0;
    return noon + hourAngle;
}

double ComputeAsrTime(double julianBase, double dayFraction, double latitude, double longitude, double tzHours, double shadowFactor) {
    double t = AsrCore(julianBase + dayFraction, latitude, longitude, tzHours, shadowFactor);
    if (std::isnan(t)) return t;
    return AsrCore(julianBase + t / 24.0, latitude, longitude, tzHours, shadowFactor);
}

TimeHM HoursToTimeHM(double hours) {
    if (std::isnan(hours) || std::isinf(hours)) {
        return {12, 0};
    }
    double fixedHours = FixHour(hours);
    int totalMinutes = static_cast<int>(std::llround(fixedHours * 60.0));
    totalMinutes = std::clamp(totalMinutes, 0, 23 * 60 + 59);
    return {totalMinutes / 60, totalMinutes % 60};
}

struct MethodParams {
    double fajrAngle;
    double ishaAngle;
    int ishaMinutesAfterMaghrib; // < 0 ise ishaAngle kullanılır
    double maghribAngle;
    bool applyDiyanetTemkin;
};

MethodParams GetMethodParams(CalculationMethod method) {
    switch (method) {
        case CalculationMethod::Jafari:           return {16.0, 14.0, -1, 4.0, false};
        case CalculationMethod::Karachi:          return {18.0, 18.0, -1, 0.833, false};
        case CalculationMethod::ISNA:             return {15.0, 15.0, -1, 0.833, false};
        case CalculationMethod::MWL:              return {18.0, 17.0, -1, 0.833, false};
        case CalculationMethod::UmmAlQura:        return {18.5, 0.0,  90, 0.833, false};
        case CalculationMethod::Egypt:            return {19.5, 17.5, -1, 0.833, false};
        case CalculationMethod::Tehran:           return {17.7, 14.0, -1, 4.5, false};
        case CalculationMethod::Gulf:             return {19.5, 0.0,  90, 0.833, false};
        case CalculationMethod::Kuwait:           return {18.0, 17.5, -1, 0.833, false};
        case CalculationMethod::Qatar:            return {18.0, 0.0,  90, 0.833, false};
        case CalculationMethod::Singapore:        return {20.0, 18.0, -1, 0.833, false};
        case CalculationMethod::FranceUOIF:       return {12.0, 12.0, -1, 0.833, false};
        case CalculationMethod::Diyanet:          return {18.0, 17.0, -1, 0.833, true};
        case CalculationMethod::Russia:           return {16.0, 15.0, -1, 0.833, false};
        case CalculationMethod::Moonsighting:     return {18.0, 18.0, -1, 0.833, false};
        case CalculationMethod::Dubai:            return {18.2, 18.2, -1, 0.833, false};
        case CalculationMethod::MalaysiaJAKIM:    return {20.0, 18.0, -1, 0.833, false};
        case CalculationMethod::Tunisia:          return {18.0, 18.0, -1, 0.833, false};
        case CalculationMethod::Algeria:          return {18.0, 17.0, -1, 0.833, false};
        case CalculationMethod::IndonesiaKEMENAG: return {20.0, 18.0, -1, 0.833, false};
        case CalculationMethod::Morocco:          return {19.0, 17.0, -1, 0.833, false};
        case CalculationMethod::Portugal:         return {18.0, 17.0, -1, 0.833, false};
        case CalculationMethod::Jordan:           return {18.0, 18.0, -1, 0.833, false};
    }
    return {18.0, 17.0, -1, 0.833, true};
}

bool FindDynamicTimeZone(std::wstring_view tzName, DYNAMIC_TIME_ZONE_INFORMATION& outDtzi) {
    if (tzName.empty()) return false;
    using EnumDynTzFn = DWORD(WINAPI*)(DWORD, PDYNAMIC_TIME_ZONE_INFORMATION);
    static EnumDynTzFn enumFn = []() -> EnumDynTzFn {
        if (HMODULE k32 = GetModuleHandleW(L"kernel32.dll")) {
            if (auto p = reinterpret_cast<EnumDynTzFn>(reinterpret_cast<void*>(GetProcAddress(k32, "EnumDynamicTimeZoneInformation")))) {
                return p;
            }
        }
        HMODULE adv = GetModuleHandleW(L"advapi32.dll");
        if (!adv) adv = LoadLibraryW(L"advapi32.dll");
        if (adv) {
            return reinterpret_cast<EnumDynTzFn>(reinterpret_cast<void*>(GetProcAddress(adv, "EnumDynamicTimeZoneInformation")));
        }
        return nullptr;
    }();
    if (!enumFn) return false;

    std::wstring target(tzName);
    DWORD idx = 0;
    while (true) {
        DYNAMIC_TIME_ZONE_INFORMATION dtzi{};
        DWORD rc = enumFn(idx++, &dtzi);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS) continue;
        if (_wcsicmp(dtzi.TimeZoneKeyName, target.c_str()) == 0 ||
            _wcsicmp(dtzi.StandardName, target.c_str()) == 0) {
            outDtzi = dtzi;
            return true;
        }
    }
    return false;
}

} // namespace

double GetTimeZoneOffsetHours(DateYMD date, std::wstring_view timeZoneId) {
    SYSTEMTIME localNoon{};
    localNoon.wYear = static_cast<WORD>(date.year);
    localNoon.wMonth = static_cast<WORD>(date.month);
    localNoon.wDay = static_cast<WORD>(date.day);
    localNoon.wHour = 12;

    TIME_ZONE_INFORMATION tzi{};
    bool hasTzi = false;

    using GetTzForYearFn = BOOL(WINAPI*)(USHORT, PDYNAMIC_TIME_ZONE_INFORMATION, LPTIME_ZONE_INFORMATION);
    static GetTzForYearFn getTzYearFn = []() -> GetTzForYearFn {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        if (!k32) return nullptr;
        return reinterpret_cast<GetTzForYearFn>(reinterpret_cast<void*>(GetProcAddress(k32, "GetTimeZoneInformationForYear")));
    }();

    DYNAMIC_TIME_ZONE_INFORMATION dtzi{};
    if (getTzYearFn && FindDynamicTimeZone(timeZoneId, dtzi)) {
        if (getTzYearFn(localNoon.wYear, &dtzi, &tzi)) {
            hasTzi = true;
        }
    }

    SYSTEMTIME utcNoon{};
    if (TzSpecificLocalTimeToSystemTime(hasTzi ? &tzi : nullptr, &localNoon, &utcNoon)) {
        FILETIME ftLocal{}, ftUtc{};
        if (SystemTimeToFileTime(&localNoon, &ftLocal) && SystemTimeToFileTime(&utcNoon, &ftUtc)) {
            ULARGE_INTEGER uLocal{{ftLocal.dwLowDateTime, ftLocal.dwHighDateTime}};
            ULARGE_INTEGER uUtc{{ftUtc.dwLowDateTime, ftUtc.dwHighDateTime}};
            int64_t diff100Ns = static_cast<int64_t>(uLocal.QuadPart) - static_cast<int64_t>(uUtc.QuadPart);
            return static_cast<double>(diff100Ns) / 36000000000.0;
        }
    }

    if (timeZoneId == L"Turkey Standard Time") return 3.0;
    return 0.0;
}

const wchar_t* GetMethodFriendlyName(CalculationMethod method) {
    switch (method) {
        case CalculationMethod::Diyanet:          return L"Diyanet İşleri Başkanlığı (Türkiye)";
        case CalculationMethod::MWL:              return L"Muslim World League (MWL — Dünya Genel)";
        case CalculationMethod::ISNA:             return L"ISNA (Kuzey Amerika)";
        case CalculationMethod::UmmAlQura:        return L"Ümmü'l-Kura Üniversitesi (Mekke)";
        case CalculationMethod::Egypt:            return L"Mısır Genel Kurumu (Egypt Survey)";
        case CalculationMethod::Karachi:          return L"Karaçi İslam Bilimleri (Pakistan/Hindistan)";
        case CalculationMethod::FranceUOIF:       return L"UOIF — Fransa İslam Kurumu (12°/12°)";
        case CalculationMethod::Moonsighting:     return L"Moonsighting Committee Worldwide";
        case CalculationMethod::Gulf:             return L"Körfez Bölgesi (Gulf Region)";
        case CalculationMethod::Kuwait:           return L"Kuveyt (Kuwait)";
        case CalculationMethod::Qatar:            return L"Katar (Qatar)";
        case CalculationMethod::Dubai:            return L"Dubai (BAE)";
        case CalculationMethod::Singapore:        return L"Singapur (MUIS)";
        case CalculationMethod::MalaysiaJAKIM:    return L"Malezya (JAKIM)";
        case CalculationMethod::IndonesiaKEMENAG: return L"Endonezya (KEMENAG)";
        case CalculationMethod::Russia:           return L"Rusya Müslümanları Dini İdaresi";
        case CalculationMethod::Tunisia:          return L"Tunus (Tunisia)";
        case CalculationMethod::Algeria:          return L"Cezayir (Algeria)";
        case CalculationMethod::Morocco:          return L"Fas (Morocco)";
        case CalculationMethod::Jordan:           return L"Ürdün Evkaf Bakanlığı";
        case CalculationMethod::Portugal:         return L"Lizbon İslam Toplumu (Portekiz)";
        case CalculationMethod::Tehran:           return L"Tahran Üniversitesi (Jeofizik Enst.)";
        case CalculationMethod::Jafari:           return L"Caferi (Leva Enstitüsü, Kum)";
    }
    return L"Diyanet İşleri Başkanlığı";
}

CalculationMethod GetRecommendedMethodForCountry(std::wstring_view countryCode) {
    std::wstring cc = NormalizeTurkishLower(TrimWide(countryCode));
    if (cc == L"tr" || cc == L"cy") return CalculationMethod::Diyanet;
    if (cc == L"us" || cc == L"ca") return CalculationMethod::ISNA;
    if (cc == L"sa") return CalculationMethod::UmmAlQura;
    if (cc == L"eg") return CalculationMethod::Egypt;
    if (cc == L"pk" || cc == L"in" || cc == L"bd" || cc == L"af") return CalculationMethod::Karachi;
    if (cc == L"ir") return CalculationMethod::Tehran;
    if (cc == L"kw") return CalculationMethod::Kuwait;
    if (cc == L"qa") return CalculationMethod::Qatar;
    if (cc == L"ae") return CalculationMethod::Dubai;
    if (cc == L"bh" || cc == L"om") return CalculationMethod::Gulf;
    if (cc == L"sg") return CalculationMethod::Singapore;
    if (cc == L"fr") return CalculationMethod::FranceUOIF;
    if (cc == L"ru") return CalculationMethod::Russia;
    if (cc == L"my" || cc == L"bn") return CalculationMethod::MalaysiaJAKIM;
    if (cc == L"tn") return CalculationMethod::Tunisia;
    if (cc == L"dz") return CalculationMethod::Algeria;
    if (cc == L"id") return CalculationMethod::IndonesiaKEMENAG;
    if (cc == L"ma") return CalculationMethod::Morocco;
    if (cc == L"pt") return CalculationMethod::Portugal;
    if (cc == L"jo") return CalculationMethod::Jordan;
    return CalculationMethod::MWL;
}

DailyPrayerTimes CalculatePrayerTimes(
    DateYMD date,
    double latitude,
    double longitude,
    std::wstring_view timeZoneId,
    CalculationMethod method,
    const PrayerOffsets& offsets,
    AsrJuristicMethod asrMethod) {

    double tzHours = GetTimeZoneOffsetHours(date, timeZoneId);
    double julianDate = ToJulianDate(date.year, date.month, date.day) - longitude / 360.0;
    MethodParams p = GetMethodParams(method);
    double asrShadowFactor = (asrMethod == AsrJuristicMethod::Hanafi) ? 2.0 : 1.0;

    double imsak = ComputeSunAngleTime(julianDate, 5.0 / 24.0, latitude, longitude, tzHours, p.fajrAngle, true);
    double gunes = ComputeSunAngleTime(julianDate, 6.0 / 24.0, latitude, longitude, tzHours, 0.833, true);
    double ogle = ComputeSolarNoon(julianDate, 12.0 / 24.0, longitude, tzHours);
    double ikindi = ComputeAsrTime(julianDate, 15.0 / 24.0, latitude, longitude, tzHours, asrShadowFactor);
    double aksam = ComputeSunAngleTime(julianDate, 18.0 / 24.0, latitude, longitude, tzHours, p.maghribAngle, false);
    if (std::isnan(aksam)) {
        aksam = ComputeSunAngleTime(julianDate, 18.0 / 24.0, latitude, longitude, tzHours, 0.833, false);
    }

    double nightHours = FixHour(gunes - aksam);
    if (std::isnan(imsak) || imsak < 0.0 || imsak >= gunes) {
        imsak = gunes - (p.fajrAngle / 60.0) * nightHours;
        if (imsak < 0.02) imsak = std::max(0.02, gunes * 0.5);
    }

    double yatsi = 0.0;
    if (p.ishaMinutesAfterMaghrib > 0) {
        yatsi = aksam + static_cast<double>(p.ishaMinutesAfterMaghrib) / 60.0;
    } else {
        yatsi = ComputeSunAngleTime(julianDate, 19.5 / 24.0, latitude, longitude, tzHours, p.ishaAngle, false);
        if (std::isnan(yatsi) || yatsi >= 23.99 || yatsi <= aksam) {
            yatsi = aksam + (p.ishaAngle / 60.0) * nightHours;
        }
    }

    if (p.applyDiyanetTemkin) {
        imsak -= 2.0 / 60.0;
        gunes -= 7.0 / 60.0;
        ogle += 5.0 / 60.0;
        ikindi += 4.0 / 60.0;
        aksam += 7.0 / 60.0;
        yatsi += 2.0 / 60.0;
    }

    imsak += offsets.imsak / 60.0;
    gunes += offsets.gunes / 60.0;
    ogle += offsets.ogle / 60.0;
    ikindi += offsets.ikindi / 60.0;
    aksam += offsets.aksam / 60.0;
    yatsi += offsets.yatsi / 60.0;

    gunes = std::clamp(gunes, 0.05, 23.90);
    imsak = std::clamp(imsak, 0.01, std::max(0.01, gunes - 1.0 / 60.0));
    aksam = std::clamp(aksam, ogle + 1.0 / 60.0, 23.95);
    yatsi = std::clamp(yatsi, std::min(23.98, aksam + 1.0 / 60.0), 23.98);

    DailyPrayerTimes out;
    out.date = date;
    out.imsak = HoursToTimeHM(imsak);
    out.gunes = HoursToTimeHM(gunes);
    out.ogle = HoursToTimeHM(ogle);
    out.ikindi = HoursToTimeHM(ikindi);
    out.aksam = HoursToTimeHM(aksam);
    out.yatsi = HoursToTimeHM(yatsi);
    out.hijriDateText = FormatTurkishHijriDate(date);
    out.source = L"Yerel Hesaplama";
    return out;
}

double CalculateQiblaBearing(double latitude, double longitude) {
    constexpr double kKaabaLat = 21.422487;
    constexpr double kKaabaLon = 39.826206;
    double lat1 = latitude * kDegToRad;
    double lat2 = kKaabaLat * kDegToRad;
    double dLon = (kKaabaLon - longitude) * kDegToRad;
    double y = std::sin(dLon) * std::cos(lat2);
    double x = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
    double bearing = std::atan2(y, x) * kRadToDeg;
    return std::fmod(bearing + 360.0, 360.0);
}

const wchar_t* GetTurkishCompassDirection(double bearingDegrees) {
    double normalized = std::fmod(std::fmod(bearingDegrees, 360.0) + 360.0, 360.0);
    int idx = static_cast<int>(std::llround(normalized / 45.0)) % 8;
    switch (idx) {
        case 0: return L"K";
        case 1: return L"KD";
        case 2: return L"D";
        case 3: return L"GD";
        case 4: return L"G";
        case 5: return L"GB";
        case 6: return L"B";
        case 7: return L"KB";
    }
    return L"K";
}

std::wstring FormatQiblaLabel(double latitude, double longitude) {
    double bearing = CalculateQiblaBearing(latitude, longitude);
    int rounded = static_cast<int>(std::llround(bearing)) % 360;
    wchar_t buf[48];
    swprintf_s(buf, L"Kıble %d° %ls", rounded, GetTurkishCompassDirection(bearing));
    return std::wstring(buf);
}

const wchar_t* TranslateHijriMonthName(int monthNumber) {
    static constexpr const wchar_t* kMonths[12] = {
        L"Muharrem", L"Safer", L"Rebiülevvel", L"Rebiülahir",
        L"Cemaziyelevvel", L"Cemaziyelahir", L"Recep", L"Şaban",
        L"Ramazan", L"Şevval", L"Zilkade", L"Zilhicce"
    };
    if (monthNumber >= 1 && monthNumber <= 12) {
        return kMonths[monthNumber - 1];
    }
    return L"";
}

HijriDateYMD GregorianToHijri(DateYMD date) {
    int64_t jdn = date.ToEpochDays() + 2440588LL;
    int64_t l = jdn - 1948440LL + 10632LL;
    int64_t n = (l - 1) / 10631LL;
    l = l - 10631LL * n + 354LL;
    int64_t j = ((10985LL - l) / 5316LL) * ((50LL * l) / 17719LL) +
                (l / 5670LL) * ((43LL * l) / 15238LL);
    l = l - ((30LL - j) / 15LL) * ((17719LL * j) / 50LL) -
        (j / 16LL) * ((15238LL * j) / 43LL) + 29LL;
    int month = static_cast<int>((24LL * l) / 709LL);
    int day = static_cast<int>(l - (709LL * month) / 24LL);
    int year = static_cast<int>(30LL * n + j - 30LL);
    return {year, month, day};
}

DateYMD FindNextOrCurrentRamadanStart(DateYMD refDate) {
    for (int y = refDate.year - 1; y <= refDate.year + 1; ++y) {
        for (const auto& r : ListReligiousDays(y)) {
            if (r.id != HolidayId::RamadanStart) continue;
            int64_t off = refDate.ToEpochDays() - r.date.ToEpochDays();
            if (off < 30) return r.date;  // current Ramadan (0..29) or the next one
        }
    }
    HijriDateYMD h = GregorianToHijri(refDate);
    if (h.month == 9) {
        DateYMD cur = refDate;
        for (int i = 0; i < 31; ++i) {
            DateYMD prev = cur.AddDays(-1);
            if (GregorianToHijri(prev).month != 9) {
                return cur;
            }
            cur = prev;
        }
        return cur;
    }

    for (int i = 1; i <= 380; ++i) {
        DateYMD cand = refDate.AddDays(i);
        HijriDateYMD hc = GregorianToHijri(cand);
        if (hc.month == 9 && hc.day == 1) {
            return cand;
        }
    }
    return refDate;
}

KerahatStatus ComputeKerahatStatus(const LocalDateTime& now, const DailyPrayerTimes& today) {
    int64_t nowSec = now.ToEpochSeconds();
    int64_t gunesSec = LocalDateTime::FromDateTime(now.date, today.gunes).ToEpochSeconds();
    if (nowSec >= gunesSec && nowSec < (gunesSec + 45LL * 60LL)) {
        return KerahatStatus::AfterSunrise;
    }

    int64_t ogleSec = LocalDateTime::FromDateTime(now.date, today.ogle).ToEpochSeconds();
    if (nowSec >= (ogleSec - 45LL * 60LL) && nowSec < ogleSec) {
        return KerahatStatus::BeforeDhuhr;
    }

    int64_t aksamSec = LocalDateTime::FromDateTime(now.date, today.aksam).ToEpochSeconds();
    if (nowSec >= (aksamSec - 45LL * 60LL) && nowSec < aksamSec) {
        return KerahatStatus::BeforeMaghrib;
    }

    return KerahatStatus::None;
}

std::wstring FormatTurkishHijriDate(DateYMD date) {
    HijriDateYMD h = GregorianToHijri(date);
    const wchar_t* mName = TranslateHijriMonthName(h.month);
    wchar_t buf[64];
    swprintf_s(buf, L"%d %ls %d", h.day, mName[0] ? mName : L"", h.year);
    return std::wstring(buf);
}

ExtraTimes ComputeExtraTimes(const DailyPrayerTimes& today, TimeHM tomorrowImsak) {
    auto fromMin = [](int m) {
        m = ((m % 1440) + 1440) % 1440;
        return TimeHM{m / 60, m % 60};
    };
    int maghrib = today.aksam.TotalMinutes();
    int night = tomorrowImsak.TotalMinutes() + 1440 - maghrib;
    ExtraTimes x;
    x.duha = fromMin(today.gunes.TotalMinutes() + 45);
    x.midnight = fromMin(maghrib + night / 2);
    x.tahajjud = fromMin(maghrib + (night * 2) / 3);
    return x;
}

namespace {

// Diyanet's published 2026 calendar (the tabular Hijri calendar can be a day off from it).
// shortcut: only 2026 is hard-coded; other years use the tabular calendar. Add a year here when Diyanet publishes it.
std::vector<ReligiousDay> Diyanet2026() {
    auto d = [](int m, int day) { return DateYMD{2026, m, day}; };
    return {
        {d(1, 15), HolidayId::Miraj, true, 1, false},
        {d(2, 2), HolidayId::Barat, true, 1, false},
        {d(2, 19), HolidayId::RamadanStart, false, 1, false},
        {d(3, 16), HolidayId::Qadr, true, 1, false},
        {d(3, 19), HolidayId::EidFitr, false, 1, true},
        {d(3, 20), HolidayId::EidFitr, false, 3, false},
        {d(5, 26), HolidayId::Arafah, false, 1, false},
        {d(5, 27), HolidayId::EidAdha, false, 4, false},
        {d(6, 16), HolidayId::HijriNewYear, false, 1, false},
        {d(6, 25), HolidayId::Ashura, false, 1, false},
        {d(8, 24), HolidayId::Mawlid, true, 1, false},
        {d(12, 10), HolidayId::Raghaib, true, 1, false},
    };
}

std::vector<ReligiousDay> TabularReligiousDays(int year) {
    struct DayType { int m, d; HolidayId id; int span; };
    static constexpr DayType kDay[] = {
        {1, 1, HolidayId::HijriNewYear, 1}, {1, 10, HolidayId::Ashura, 1}, {9, 1, HolidayId::RamadanStart, 1},
        {10, 1, HolidayId::EidFitr, 3},     {12, 9, HolidayId::Arafah, 1}, {12, 10, HolidayId::EidAdha, 4},
    };
    struct NightType { int m, d; HolidayId id; };
    static constexpr NightType kNight[] = {
        {3, 12, HolidayId::Mawlid}, {7, 27, HolidayId::Miraj}, {8, 15, HolidayId::Barat}, {9, 27, HolidayId::Qadr},
    };
    std::vector<ReligiousDay> out;
    for (DateYMD d{year, 1, 1}; d.year == year; d = d.AddDays(1)) {
        HijriDateYMD h = GregorianToHijri(d);
        for (const auto& e : kDay) {
            if (h.month != e.m || h.day != e.d) continue;
            if (e.id == HolidayId::EidFitr) out.push_back({d.AddDays(-1), e.id, false, 1, true});
            out.push_back({d, e.id, false, e.span, false});
        }
        // Kandil nights: the evening before the Hijri day starts.
        for (const auto& e : kNight) {
            if (h.month == e.m && h.day == e.d) out.push_back({d.AddDays(-1), e.id, true, 1, false});
        }
        // Raghaib: the night before the first Friday of Rajab.
        if (h.month == 7 && h.day <= 7 && d.DayOfWeek() == 5) out.push_back({d.AddDays(-1), HolidayId::Raghaib, true, 1, false});
    }
    return out;
}

} // namespace

const std::vector<ReligiousDay>& ListReligiousDays(int year) {
    static std::map<int, std::vector<ReligiousDay>> cache;
    auto it = cache.find(year);
    if (it != cache.end()) return it->second;
    std::vector<ReligiousDay> v = (year == 2026) ? Diyanet2026() : TabularReligiousDays(year);
    std::sort(v.begin(), v.end(), [](const ReligiousDay& a, const ReligiousDay& b) {
        return a.date.ToEpochDays() < b.date.ToEpochDays();
    });
    return cache.emplace(year, std::move(v)).first->second;
}

HolidayEve FindHolidayStartingTomorrow(DateYMD today) {
    DateYMD tomorrow = today.AddDays(1);
    for (int y : {today.year, tomorrow.year}) {
        for (const auto& r : ListReligiousDays(y)) {
            if (r.isArife) continue;
            if (r.isNight ? (r.date == today) : (r.date == tomorrow)) return {r.id, r.isNight};
        }
    }
    return {};
}

} // namespace vakit
