#pragma once

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

std::wstring FormatTurkishHijriDate(DateYMD date);
const wchar_t* TranslateHijriMonthName(int monthNumber);

} // namespace vakit
