#pragma once

#include <vector>

#include "models.h"
#include <string>
#include <string_view>

namespace vakit {

double GetTimeZoneOffsetHours(DateYMD date, std::wstring_view timeZoneId);

const wchar_t* GetMethodFriendlyName(CalculationMethod method);
CalculationMethod GetRecommendedMethodForCountry(std::wstring_view countryCode);

DailyPrayerTimes CalculatePrayerTimes(
    DateYMD date,
    double latitude,
    double longitude,
    std::wstring_view timeZoneId,
    CalculationMethod method,
    const PrayerOffsets& offsets = {},
    AsrJuristicMethod asrMethod = AsrJuristicMethod::Standard);

double CalculateQiblaBearing(double latitude, double longitude);
const wchar_t* GetTurkishCompassDirection(double bearingDegrees);
std::wstring FormatQiblaLabel(double latitude, double longitude);

struct HijriDateYMD {
    int year = 1448;
    int month = 1;
    int day = 1;
};

HijriDateYMD GregorianToHijri(DateYMD date);
DateYMD FindNextOrCurrentRamadanStart(DateYMD refDate);
KerahatStatus ComputeKerahatStatus(const LocalDateTime& now, const DailyPrayerTimes& today);

// Optional extra times. Duha starts when the post-sunrise makruh window (45 min, see ComputeKerahatStatus) ends.
struct ExtraTimes {
    TimeHM duha;      // Kuşluk / Ishraq
    TimeHM midnight;  // Islamic midnight: half of Maghrib -> next Fajr
    TimeHM tahajjud;  // start of the last third of the night
};
ExtraTimes ComputeExtraTimes(const DailyPrayerTimes& today, TimeHM tomorrowImsak);

enum class HolidayId : int {
    None = 0, HijriNewYear, Ashura, Mawlid, Raghaib, Miraj, Barat, RamadanStart, Qadr, EidFitr, Arafah, EidAdha
};
struct HolidayEve {
    HolidayId id = HolidayId::None;
    bool isNight = false;  // kandil-type nights are observed on the evening of `today`
};
// Holiday whose day starts tomorrow (Diyanet table where known, otherwise tabular Hijri calendar; may differ from local sighting by a day).
HolidayEve FindHolidayStartingTomorrow(DateYMD today);

// One religious day/night on the Gregorian calendar. Kandil-type nights carry the evening's date (as Diyanet lists them).
struct ReligiousDay {
    DateYMD date;
    HolidayId id = HolidayId::None;
    bool isNight = false;
    int span = 1;            // Eid lasts 3 (Fitr) / 4 (Adha) days from `date`
    bool isArife = false;    // eve of Ramadan Eid (id == EidFitr)
};
const std::vector<ReligiousDay>& ListReligiousDays(int gregorianYear);

std::wstring FormatTurkishHijriDate(DateYMD date);
const wchar_t* TranslateHijriMonthName(int monthNumber);

} // namespace vakit
