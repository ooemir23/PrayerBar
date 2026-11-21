#pragma once

#include "models.h"
#include <string>

namespace vakit {

struct FormattedTaskbarDisplay {
    std::wstring line1;
    std::wstring line2;
    bool hasLine2 = false;
    bool showPhaseIcon = false;
    bool showDot = false;
    PrayerKind nextPrayer = PrayerKind::Imsak;
    double progress = 0.0;
    bool isUrgent = false;
};

FormattedTaskbarDisplay FormatTaskbar(const PrayerCountdownState& state, const AppSettings& settings);
std::wstring FormatRemainingLong(int64_t remainingSeconds, bool showSeconds);
std::wstring FormatRemainingLong(int64_t remainingSeconds, bool showSeconds, AppLanguage lang);
std::wstring FormatRemainingCompact(int64_t remainingSeconds, bool showSeconds);
std::wstring FormatRemainingCompact(int64_t remainingSeconds, bool showSeconds, AppLanguage lang);
std::wstring FormatRemainingHhMm(int64_t remainingSeconds, bool showSeconds);
std::pair<std::wstring, std::wstring> FormatTrayText(const PrayerCountdownState& state);
std::pair<std::wstring, std::wstring> FormatTrayText(const PrayerCountdownState& state, const AppSettings& settings);
std::wstring FormatTrayTooltip(const PrayerCountdownState& state, const AppSettings& settings);
const wchar_t* GetStyleFriendlyName(TaskbarDisplayStyle style);

} // namespace vakit
