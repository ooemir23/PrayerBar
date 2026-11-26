#include "schedule_service.h"
#include "city_catalog.h"
#include "json_mini.h"
#include "prayer_calc.h"
#include "settings_store.h"
#include "utf_util.h"
#include "../platform/win_http.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <windows.h>
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

std::wstring MapIanaToWindowsTimeZone(std::string_view iana) {
    if (iana == "Europe/Istanbul" || iana == "Asia/Istanbul") return L"Turkey Standard Time";
    if (iana == "Europe/Berlin" || iana == "Europe/Vienna" || iana == "Europe/Amsterdam" ||
        iana == "Europe/Rome" || iana == "Europe/Stockholm" || iana == "Europe/Copenhagen" ||
        iana == "Europe/Oslo" || iana == "Europe/Zurich" || iana == "Europe/Madrid") return L"W. Europe Standard Time";
    if (iana == "Europe/Paris" || iana == "Europe/Brussels" || iana == "Europe/Luxembourg") return L"Romance Standard Time";
    if (iana == "Europe/London" || iana == "Europe/Lisbon" || iana == "Europe/Dublin") return L"GMT Standard Time";
    if (iana == "Europe/Sarajevo" || iana == "Europe/Prague" || iana == "Europe/Warsaw" ||
        iana == "Europe/Budapest" || iana == "Europe/Belgrade" || iana == "Europe/Zagreb") return L"Central European Standard Time";
    if (iana == "Europe/Athens" || iana == "Europe/Helsinki" || iana == "Europe/Bucharest" ||
        iana == "Europe/Sofia" || iana == "Europe/Kyiv" || iana == "Europe/Riga" ||
        iana == "Europe/Tallinn" || iana == "Europe/Vilnius") return L"FLE Standard Time";
    if (iana == "Europe/Moscow" || iana == "Europe/Minsk") return L"Russian Standard Time";
    if (iana == "Asia/Baku") return L"Azerbaijan Standard Time";
    if (iana == "Asia/Riyadh" || iana == "Asia/Qatar" || iana == "Asia/Kuwait" ||
        iana == "Asia/Baghdad") return L"Arab Standard Time";
    if (iana == "Asia/Dubai" || iana == "Asia/Muscat") return L"Arabian Standard Time";
    if (iana == "Asia/Amman") return L"Jordan Standard Time";
    if (iana == "Asia/Tehran") return L"Iran Standard Time";
    if (iana == "Asia/Karachi") return L"Pakistan Standard Time";
    if (iana == "Asia/Kabul") return L"Afghanistan Standard Time";
    if (iana == "Asia/Kolkata" || iana == "Asia/Calcutta") return L"India Standard Time";
    if (iana == "Asia/Dhaka") return L"Bangladesh Standard Time";
    if (iana == "Asia/Kathmandu") return L"Nepal Standard Time";
    if (iana == "Asia/Kuala_Lumpur" || iana == "Asia/Singapore" || iana == "Asia/Manila") return L"Singapore Standard Time";
    if (iana == "Asia/Jakarta" || iana == "Asia/Bangkok" || iana == "Asia/Ho_Chi_Minh") return L"SE Asia Standard Time";
    if (iana == "Asia/Tashkent" || iana == "Asia/Almaty") return L"Central Asia Standard Time";
    if (iana == "Asia/Bishkek") return L"Central Asia Standard Time";
    if (iana == "Asia/Dushanbe") return L"West Asia Standard Time";
    if (iana == "Asia/Yekaterinburg") return L"Ekaterinburg Standard Time";
    if (iana == "Asia/Krasnoyarsk") return L"North Asia Standard Time";
    if (iana == "Asia/Vladivostok") return L"Vladivostok Standard Time";
    if (iana == "Asia/Shanghai" || iana == "Asia/Hong_Kong" || iana == "Asia/Urumqi") return L"China Standard Time";
    if (iana == "Asia/Jerusalem") return L"Israel Standard Time";
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
    if (iana == "America/New_York" || iana == "America/Toronto" || iana == "America/Detroit") return L"Eastern Standard Time";
    if (iana == "America/Chicago" || iana == "America/Winnipeg" || iana == "America/Mexico_City" ||
        iana == "America/Monterrey") return L"Central Standard Time";
    if (iana == "America/Regina") return L"Canada Central Standard Time";
    if (iana == "America/Denver") return L"Mountain Standard Time";
    if (iana == "America/Phoenix") return L"US Mountain Standard Time";
    if (iana == "America/Los_Angeles" || iana == "America/Vancouver") return L"Pacific Standard Time";
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

std::optional<std::pair<double, double>> GetWindowsLocationCoordinates() {
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initResult)) return std::nullopt;

    std::optional<std::pair<double, double>> coordinates;
    ILocation* location = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_Location, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ILocation, reinterpret_cast<void**>(&location))) && location) {
        ILocationReport* baseReport = nullptr;
        if (SUCCEEDED(location->GetReport(IID_ILatLongReport, &baseReport)) && baseReport) {
            auto* report = static_cast<ILatLongReport*>(baseReport);
            DOUBLE latitude = 0.0;
            DOUBLE longitude = 0.0;
            if (SUCCEEDED(report->GetLatitude(&latitude)) && SUCCEEDED(report->GetLongitude(&longitude)) &&
                latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0) {
                coordinates = std::make_pair(latitude, longitude);
            }
            baseReport->Release();
        }
        location->Release();
    }

    CoUninitialize();
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

DailyPrayerTimes ScheduleService::GetOrCalculateDay(DateYMD date, const AppSettings& settings, bool triggerSync) {
    std::wstring monthKey = BuildMonthKey(date.year, date.month, settings);
    std::map<DateYMD, DailyPrayerTimes> monthMap;
    if (TryLoadMonth(monthKey, monthMap)) {
        auto it = monthMap.find(date);
        if (it != monthMap.end()) {
            return ApplyOffsets(it->second, settings.offsets);
        }
    }

    if (triggerSync) {
        TriggerBackgroundFetch(monthKey, date.year, date.month, settings);
    }

    return CalculatePrayerTimes(
        date,
        settings.latitude,
        settings.longitude,
        settings.timeZoneId,
        settings.calculationMethod,
        settings.offsets,
        settings.asrMethod);
}

bool ScheduleService::TryLoadMonth(const std::wstring& monthKey, std::map<DateYMD, DailyPrayerTimes>& outMap) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = memoryMonths_.find(monthKey);
        if (it != memoryMonths_.end()) {
            outMap = it->second;
            return true;
        }
    }

    std::wstring path = cacheDir_ + L"\\schedule_" + monthKey + L".json";
    std::string raw;
    if (!ReadFileUtf8(path, raw)) return false;

    auto parsed = JsonParse(raw);
    if (!parsed || parsed->type != JsonValue::Type::Object) return false;
    const JsonValue* daysArr = parsed->Find("days");
    if (!daysArr || daysArr->type != JsonValue::Type::Array || daysArr->arrVal.empty()) return false;

    std::map<DateYMD, DailyPrayerTimes> loaded;
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

    if (loaded.empty()) return false;
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

std::optional<DetectedLocationResult> ScheduleService::DetectCurrentLocation() {
    auto windowsCoordinates = GetWindowsLocationCoordinates();
    if (windowsCoordinates) {
        if (auto nearest = FindNearestCity(windowsCoordinates->first, windowsCoordinates->second, 30.0)) {
            DetectedLocationResult result;
            result.cityId = nearest->id;
            result.cityName = nearest->name;
            result.countryName = nearest->country;
            result.countryCode = nearest->countryCode;
            result.latitude = windowsCoordinates->first;
            result.longitude = windowsCoordinates->second;
            result.timeZoneId = nearest->timeZoneId;
            result.recommendedMethod = GetRecommendedMethodForCountry(result.countryCode);
            result.sourceLabel = L"Windows Konum";
            return result;
        }
    }

    auto parseAndBuild = [&](double ipLatitude, double ipLongitude, std::string_view city, std::string_view country,
                             std::string_view cc, std::string_view ianaTz, const wchar_t* source) -> DetectedLocationResult {
        double lat = windowsCoordinates ? windowsCoordinates->first : ipLatitude;
        double lon = windowsCoordinates ? windowsCoordinates->second : ipLongitude;
        auto nearest = FindNearestCity(lat, lon, 30.0);
        std::wstring winTz = nearest ? nearest->timeZoneId : MapIanaToWindowsTimeZone(ianaTz);
        if (winTz.empty()) winTz = MapIanaToWindowsTimeZone(ianaTz);
        if (winTz.empty()) winTz = GetSystemTimeZoneId();

        DetectedLocationResult r;
        r.cityId = nearest ? nearest->id : L"custom";
        r.cityName = !city.empty() ? Utf8ToWide(city) : (nearest ? nearest->name : L"Konum");
        r.countryName = !country.empty() ? Utf8ToWide(country) : (nearest ? nearest->country : L"Konum");
        r.countryCode = !cc.empty() ? Utf8ToWide(cc) : (nearest ? nearest->countryCode : ResolveCountryCodeFromName(r.countryName));
        r.latitude = lat;
        r.longitude = lon;
        r.timeZoneId = winTz;
        r.recommendedMethod = GetRecommendedMethodForCountry(r.countryCode);
        r.sourceLabel = windowsCoordinates ? L"Windows Konum + IP" : source;
        return r;
    };

    if (auto body = WinHttpGetUtf8(L"ipwho.is", L"/", true, 6000)) {
        if (auto doc = JsonParse(*body); doc && doc->type == JsonValue::Type::Object) {
            if (doc->GetBool("success", true)) {
                double lat = doc->GetDouble("latitude", 999.0);
                double lon = doc->GetDouble("longitude", 999.0);
                if (lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0) {
                    std::string tz;
                    if (const JsonValue* tzObj = doc->Find("timezone")) {
                        tz = tzObj->GetString("id");
                    }
                    return parseAndBuild(lat, lon, doc->GetString("city"), doc->GetString("country"),
                                         doc->GetString("country_code"), tz, L"IP Konumu (ipwho.is)");
                }
            }
        }
    }

    if (auto body = WinHttpGetUtf8(L"get.geojs.io", L"/v1/ip/geo.json", true, 6000)) {
        if (auto doc = JsonParse(*body); doc && doc->type == JsonValue::Type::Object) {
            double lat = doc->GetDouble("latitude", 999.0);
            double lon = doc->GetDouble("longitude", 999.0);
            if (lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0) {
                return parseAndBuild(lat, lon, doc->GetString("city"), doc->GetString("country"),
                                     doc->GetString("country_code"), doc->GetString("timezone"), L"IP Konumu (GeoJS)");
            }
        }
    }

    if (windowsCoordinates) {
        DetectedLocationResult result;
        result.cityId = L"custom";
        result.cityName = L"Konum";
        result.countryName = L"Konum";
        result.latitude = windowsCoordinates->first;
        result.longitude = windowsCoordinates->second;
        result.timeZoneId = GetSystemTimeZoneId();
        result.recommendedMethod = CalculationMethod::MWL;
        result.sourceLabel = L"Windows Konum";
        return result;
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
