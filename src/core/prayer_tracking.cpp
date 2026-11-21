#include "prayer_tracking.h"
#include "json_mini.h"
#include "settings_store.h"

#include <cstdio>
#include <map>
#include <mutex>

namespace vakit::tracking {

namespace {

constexpr int kKeepDays = 400;

std::mutex g_mutex;
std::wstring g_dir;
bool g_loaded = false;
std::map<int64_t, int> g_masks;  // epoch day -> bitmask by PrayerKind index

std::wstring FilePath() {
    return (g_dir.empty() ? GetDefaultLocalAppDataDir() : g_dir) + L"\\tracking.json";
}

void LoadLocked() {
    if (g_loaded) return;
    g_loaded = true;
    g_masks.clear();
    std::string raw;
    if (!ReadFileUtf8(FilePath(), raw)) return;
    auto doc = JsonParse(raw);
    if (!doc || doc->type != JsonValue::Type::Object) return;
    for (const auto& [key, val] : doc->objVal) {
        if (val.type != JsonValue::Type::Number) continue;
        long long day = 0;
        if (std::sscanf(key.c_str(), "%lld", &day) == 1) g_masks[day] = static_cast<int>(val.numVal) & 0x3F;
    }
}

void SaveLocked(int64_t todayEpoch) {
    JsonValue root = JsonValue::MakeObject();
    for (const auto& [day, mask] : g_masks) {
        if (day < todayEpoch - kKeepDays || mask == 0) continue;
        root.Set(std::to_string(day), JsonValue::MakeInt(mask));
    }
    WriteFileAtomicallyUtf8(FilePath(), JsonSerialize(root, false));
}

int Bit(PrayerKind kind) { return 1 << static_cast<int>(kind); }

} // namespace

bool IsPrayed(DateYMD date, PrayerKind kind) {
    std::lock_guard<std::mutex> lk(g_mutex);
    LoadLocked();
    auto it = g_masks.find(date.ToEpochDays());
    return it != g_masks.end() && (it->second & Bit(kind)) != 0;
}

void TogglePrayed(DateYMD date, PrayerKind kind) {
    if (kind == PrayerKind::Gunes) return;
    std::lock_guard<std::mutex> lk(g_mutex);
    LoadLocked();
    g_masks[date.ToEpochDays()] ^= Bit(kind);
    SaveLocked(date.ToEpochDays());
}

int CountPrayedInLastDays(DateYMD today, int days) {
    std::lock_guard<std::mutex> lk(g_mutex);
    LoadLocked();
    int total = 0;
    int64_t end = today.ToEpochDays();
    for (int64_t d = end - days + 1; d <= end; ++d) {
        auto it = g_masks.find(d);
        if (it == g_masks.end()) continue;
        int mask = it->second & ~Bit(PrayerKind::Gunes);
        for (int b = 0; b < 6; ++b) total += (mask >> b) & 1;
    }
    return total;
}

void SetStorageDirForTesting(const std::wstring& dir) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_dir = dir;
    g_loaded = false;
}

} // namespace vakit::tracking
