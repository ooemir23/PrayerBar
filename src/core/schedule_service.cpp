#include "schedule_service.h"
#include "city_catalog.h"
#include "json_mini.h"
#include "prayer_calc.h"
#include "settings_store.h"
#include "utf_util.h"
#include "../platform/win_http.h"

#include <algorithm>
#include <cstdio>
#include <thread>

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
    if (iana == "Europe/Berlin" || iana == "Europe/Vienna" || iana == "Europe/Amsterdam") return L"W. Europe Standard Time";
    if (iana == "Europe/Paris" || iana == "Europe/Brussels") return L"Romance Standard Time";
    if (iana == "Europe/London" || iana == "Europe/Lisbon") return L"GMT Standard Time";
    if (iana == "Europe/Sarajevo") return L"Central European Standard Time";
    if (iana == "Europe/Moscow") return L"Russian Standard Time";
    if (iana == "Asia/Baku") return L"Azerbaijan Standard Time";
    if (iana == "Asia/Riyadh" || iana == "Asia/Qatar" || iana == "Asia/Kuwait") return L"Arab Standard Time";
    if (iana == "Asia/Dubai") return L"Arabian Standard Time";
    if (iana == "Asia/Amman") return L"Jordan Standard Time";
    if (iana == "Asia/Tehran") return L"Iran Standard Time";
    if (iana == "Asia/Karachi") return L"Pakistan Standard Time";
    if (iana == "Asia/Kuala_Lumpur" || iana == "Asia/Singapore") return L"Singapore Standard Time";
    if (iana == "Asia/Jakarta") return L"SE Asia Standard Time";
    if (iana == "Africa/Cairo") return L"Egypt Standard Time";
    if (iana == "Africa/Casablanca") return L"Morocco Standard Time";
    if (iana == "Africa/Tunis" || iana == "Africa/Algiers") return L"W. Central Africa Standard Time";
    if (iana == "America/New_York" || iana == "America/Toronto") return L"Eastern Standard Time";
    if (iana == "America/Chicago") return L"Central Standard Time";
    if (iana == "Asia/Tokyo") return L"Tokyo Standard Time";
    if (iana == "Australia/Sydney") return L"AUS Eastern Standard Time";
    return L"";
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
    wchar_t buf[128];
    if (settings.useCustomCoordinates) {
        wchar_t coordBuf[64];
        swprintf_s(coordBuf, L"%.2f_%.2f", settings.latitude, settings.longitude);
        for (wchar_t* p = coordBuf; *p; ++p) {
            if (*p == L'.' || *p == L',') *p = L'p';
        }
        swprintf_s(buf, L"%ls_%d_%d_%04d%02d", coordBuf,
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

std::optional<DetectedLocationResult> ScheduleService::DetectLocationByIp() {
    auto parseAndBuild = [](double lat, double lon, std::string_view city, std::string_view country,
                            std::string_view cc, std::string_view ianaTz, const wchar_t* source) -> DetectedLocationResult {
        auto nearest = FindNearestCity(lat, lon, 30.0);
        std::wstring winTz = MapIanaToWindowsTimeZone(ianaTz);
        if (winTz.empty() && nearest) winTz = nearest->timeZoneId;

        DetectedLocationResult r;
        r.cityId = nearest ? nearest->id : L"custom";
        r.cityName = !city.empty() ? Utf8ToWide(city) : (nearest ? nearest->name : L"Konum");
        r.countryName = !country.empty() ? Utf8ToWide(country) : (nearest ? nearest->country : L"Konum");
        r.countryCode = !cc.empty() ? Utf8ToWide(cc) : (nearest ? nearest->countryCode : ResolveCountryCodeFromName(r.countryName));
        r.latitude = lat;
        r.longitude = lon;
        r.timeZoneId = winTz;
        r.recommendedMethod = GetRecommendedMethodForCountry(r.countryCode);
        r.sourceLabel = source;
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

    return std::nullopt;
}

} // namespace vakit
