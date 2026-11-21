#pragma once

#include "models.h"

#include <string>

namespace vakit::tracking {

// "Prayed" marks for the five obligatory prayers, one small JSON file (date -> bitmask).
// Loaded lazily once, written only when the user toggles a mark.
bool IsPrayed(DateYMD date, PrayerKind kind);
void TogglePrayed(DateYMD date, PrayerKind kind);
int CountPrayedInLastDays(DateYMD today, int days);  // Güneş is never counted
void SetStorageDirForTesting(const std::wstring& dir);

} // namespace vakit::tracking
