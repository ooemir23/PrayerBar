#pragma once

#include "models.h"
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace vakit {

class ScheduleService {
public:
    explicit ScheduleService(std::wstring customCacheDir = {});

    void SetRefreshedCallback(std::function<void()> cb) { onRefreshed_ = std::move(cb); }

    PrayerCountdownState ComputeState(const LocalDateTime& nowLocal, const AppSettings& settings);
    DailyPrayerTimes GetDaySchedule(DateYMD date, const AppSettings& settings);
    std::vector<DailyPrayerTimes> GetMonthSchedule(int year, int month, const AppSettings& settings, bool triggerSync = false);
    void TrimMemoryCache(DateYMD keepDate);
    bool ForceRefreshMonth(DateYMD date, const AppSettings& settings);
    void SaveMonthToCache(const std::wstring& monthKey, const std::vector<DailyPrayerTimes>& days);

    static std::wstring BuildMonthKey(int year, int month, const AppSettings& settings);
    static std::optional<DetectedLocationResult> DetectCurrentLocation();
    static std::vector<CityLocation> SearchWorldwideCities(std::wstring_view query);
    static LocalDateTime GetCurrentLocalDateTime();

private:
    DailyPrayerTimes GetOrCalculateDay(DateYMD date, const AppSettings& settings, bool triggerSync);
    bool TryGetCachedDay(const std::wstring& monthKey, DateYMD date, DailyPrayerTimes& outDay);
    bool TryLoadMonth(const std::wstring& monthKey, std::map<DateYMD, DailyPrayerTimes>& outMap);
    void TriggerBackgroundFetch(const std::wstring& monthKey, int year, int month, const AppSettings& settings);
    static std::optional<std::vector<DailyPrayerTimes>> FetchAladhanMonth(
        int year, int month, double lat, double lon, CalculationMethod method, AsrJuristicMethod asrMethod);

    struct ComputedDayEntry {
        std::wstring monthKey;
        DateYMD date{};
        PrayerOffsets offsets{};
        DailyPrayerTimes times{};
    };

    std::wstring cacheDir_;
    std::mutex mutex_;
    std::map<std::wstring, std::map<DateYMD, DailyPrayerTimes>> memoryMonths_;
    std::set<std::wstring> missingDiskMonths_;
    std::vector<ComputedDayEntry> computedDays_;
    std::set<std::wstring> inFlightKeys_;
    std::function<void()> onRefreshed_;
};

} // namespace vakit
