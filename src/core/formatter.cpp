#include "formatter.h"
#include "i18n.h"
#include "prayer_calc.h"
#include "utf_util.h"

#include <algorithm>
#include <cstdio>

namespace vakit {

namespace {

void ReplaceAll(std::wstring& target, std::wstring_view from, std::wstring_view to) {
    size_t pos = 0;
    std::wstring lowerTarget = NormalizeTurkishLower(target);
    std::wstring lowerFrom = NormalizeTurkishLower(from);
    while ((pos = lowerTarget.find(lowerFrom, pos)) != std::wstring::npos) {
        target.replace(pos, from.size(), to);
        lowerTarget.replace(pos, from.size(), lowerFrom);
        pos += to.size();
    }
}

FormattedTaskbarDisplay FormatCustom(
    std::wstring_view tpl,
    std::wstring_view vakit,
    std::wstring_view vakitE,
    std::wstring_view kisa,
    std::wstring_view kalan,
    std::wstring_view kalanKisa,
    std::wstring_view saat,
    std::wstring_view hhmm,
    const PrayerCountdownState& state) {

    std::wstring raw = TrimWide(tpl).empty() ? std::wstring(L"{kisa}→{kalan_kisa}") : std::wstring(tpl);
    ReplaceAll(raw, L"\\n", L"\n");
    ReplaceAll(raw, L"{vakit_e}", vakitE);
    ReplaceAll(raw, L"{vakit}", vakit);
    ReplaceAll(raw, L"{kisa}", kisa);
    ReplaceAll(raw, L"{kalan_kisa}", kalanKisa);
    ReplaceAll(raw, L"{kalan}", kalan);
    ReplaceAll(raw, L"{saat}", saat);
    ReplaceAll(raw, L"{hhmm}", hhmm);

    FormattedTaskbarDisplay out;
    out.nextPrayer = state.nextPrayer;
    out.progress = state.progress;
    out.isUrgent = state.isUrgent;

    size_t nl = raw.find(L'\n');
    if (nl == std::wstring::npos) {
        out.line1 = TrimWide(raw);
    } else {
        out.line1 = TrimWide(std::wstring_view(raw).substr(0, nl));
        out.line2 = TrimWide(std::wstring_view(raw).substr(nl + 1));
        out.hasLine2 = !out.line2.empty();
    }
    if (out.line1.empty()) {
        out.line1 = std::wstring(kisa) + L"→" + std::wstring(kalanKisa);
    }
    return out;
}

struct TimeUnits {
    const wchar_t* hrShort;
    const wchar_t* hrExact;
    const wchar_t* minLong;
    const wchar_t* minShort;
    const wchar_t* secShort;
    const wchar_t* lt1Long;
    const wchar_t* lt1Short;
    const wchar_t* trayHrSuffix;
};

TimeUnits UnitsForLang(AppLanguage lang) {
    switch (ResolveEffectiveLanguage(lang)) {
        case AppLanguage::Turkish:    return {L"s", L"sa", L"dk", L"d", L"sn", L"<1 dk", L"<1Dk", L"s"};
        case AppLanguage::English:    return {L"h", L"hr", L"min", L"m", L"s", L"<1 min", L"<1m", L"h"};
        case AppLanguage::Arabic:     return {L"س", L"س", L"د", L"د", L"ث", L"<1 د", L"<1د", L"س"};
        case AppLanguage::German:     return {L"Std", L"Std", L"Min", L"m", L"s", L"<1 Min", L"<1m", L"h"};
        case AppLanguage::French:     return {L"h", L"h", L"min", L"m", L"s", L"<1 min", L"<1m", L"h"};
        case AppLanguage::Indonesian: return {L"j", L"jam", L"mnt", L"m", L"d", L"<1 mnt", L"<1m", L"j"};
        case AppLanguage::Urdu:       return {L"گ", L"گھنٹے", L"منٹ", L"م", L"س", L"<1 منٹ", L"<1م", L"گ"};
        case AppLanguage::Bengali:    return {L"ঘ", L"ঘণ্টা", L"মিনিট", L"মি", L"সে", L"<1 মিনিট", L"<1মি", L"ঘ"};
        case AppLanguage::Malay:      return {L"j", L"jam", L"min", L"m", L"s", L"<1 min", L"<1m", L"j"};
        case AppLanguage::Persian:    return {L"س", L"ساعت", L"دقیقه", L"د", L"ث", L"<1 دقیقه", L"<1د", L"س"};
        case AppLanguage::Russian:    return {L"ч", L"ч", L"мин", L"м", L"с", L"<1 мин", L"<1м", L"ч"};
        case AppLanguage::Spanish:    return {L"h", L"h", L"min", L"m", L"s", L"<1 min", L"<1m", L"h"};
        default:                      return {L"s", L"sa", L"dk", L"d", L"sn", L"<1 dk", L"<1Dk", L"s"};
    }
}

// Digits are bidi-neutral: in an LTR-drawn surface "23 منٹ 00 س" gets reordered. An RTL embedding
// (RLE ... PDF) makes Arabic/Urdu/Persian countdowns read right-to-left as one phrase.
std::wstring WrapRtl(std::wstring s, AppLanguage eff) {
    if (!IsRtlLanguage(eff)) return s;
    return L"\u202B" + s + L"\u202C";
}

std::wstring FormatRemainingLongImpl(int64_t remainingSeconds, bool showSeconds, AppLanguage lang) {
    int64_t clamped = std::max<int64_t>(0, remainingSeconds);
    AppLanguage eff = ResolveEffectiveLanguage(lang);
    TimeUnits u = UnitsForLang(eff);
    wchar_t buf[64];

    if (showSeconds) {
        int totalHours = static_cast<int>(clamped / 3600);
        int mins = static_cast<int>((clamped % 3600) / 60);
        int secs = static_cast<int>(clamped % 60);
        if (totalHours > 0) {
            if (eff == AppLanguage::Turkish) {
                swprintf_s(buf, L"%ds %02dd %02dsn", totalHours, mins, secs);
            } else {
                swprintf_s(buf, L"%d%ls %02d%ls %02d%ls", totalHours, u.hrShort, mins, u.minShort, secs, u.secShort);
            }
        } else {
            swprintf_s(buf, L"%d %ls %02d %ls", mins, u.minLong, secs, u.secShort);
        }
        return std::wstring(buf);
    }

    int totalMinutes = static_cast<int>((clamped + 59) / 60);
    if (totalMinutes <= 0) return u.lt1Long;
    int hours = totalMinutes / 60;
    int mins = totalMinutes % 60;
    if (hours == 0) {
        swprintf_s(buf, L"%d %ls", mins, u.minLong);
    } else if (mins == 0) {
        swprintf_s(buf, L"%d %ls", hours, u.hrExact);
    } else if (eff == AppLanguage::Turkish) {
        swprintf_s(buf, L"%ds %ddk", hours, mins);
    } else {
        swprintf_s(buf, L"%d%ls %d%ls", hours, u.hrShort, mins, u.minLong);
    }
    return std::wstring(buf);
}

std::wstring FormatRemainingCompactImpl(int64_t remainingSeconds, bool showSeconds, AppLanguage lang);

} // namespace

std::wstring FormatRemainingLong(int64_t remainingSeconds, bool showSeconds, AppLanguage lang) {
    return WrapRtl(FormatRemainingLongImpl(remainingSeconds, showSeconds, lang), ResolveEffectiveLanguage(lang));
}

std::wstring FormatRemainingCompact(int64_t remainingSeconds, bool showSeconds, AppLanguage lang) {
    return WrapRtl(FormatRemainingCompactImpl(remainingSeconds, showSeconds, lang), ResolveEffectiveLanguage(lang));
}

std::wstring FormatRemainingLong(int64_t remainingSeconds, bool showSeconds) {
    return FormatRemainingLong(remainingSeconds, showSeconds, AppLanguage::Turkish);
}

namespace {

std::wstring FormatRemainingCompactImpl(int64_t remainingSeconds, bool showSeconds, AppLanguage lang) {
    int64_t clamped = std::max<int64_t>(0, remainingSeconds);
    AppLanguage eff = ResolveEffectiveLanguage(lang);
    TimeUnits u = UnitsForLang(eff);
    wchar_t buf[64];

    if (showSeconds) {
        int totalHours = static_cast<int>(clamped / 3600);
        int mins = static_cast<int>((clamped % 3600) / 60);
        int secs = static_cast<int>(clamped % 60);
        if (totalHours > 0) {
            swprintf_s(buf, L"%d%ls%02d%ls%02d", totalHours, u.hrShort, mins, u.minShort, secs);
        } else {
            swprintf_s(buf, L"%d:%02d", mins, secs);
        }
        return std::wstring(buf);
    }

    int totalMinutes = static_cast<int>((clamped + 59) / 60);
    if (totalMinutes <= 0) return u.lt1Short;
    int hours = totalMinutes / 60;
    int mins = totalMinutes % 60;
    if (hours == 0) {
        if (eff == AppLanguage::Turkish) {
            swprintf_s(buf, L"%dDk", mins);
        } else {
            swprintf_s(buf, L"%d%ls", mins, u.minShort);
        }
    } else if (mins == 0) {
        if (eff == AppLanguage::Turkish) {
            swprintf_s(buf, L"%dSa", hours);
        } else {
            swprintf_s(buf, L"%d%ls", hours, u.hrShort);
        }
    } else {
        swprintf_s(buf, L"%d%ls%02d%ls", hours, u.hrShort, mins, u.minShort);
    }
    return std::wstring(buf);
}

} // namespace

std::wstring FormatRemainingCompact(int64_t remainingSeconds, bool showSeconds) {
    return FormatRemainingCompact(remainingSeconds, showSeconds, AppLanguage::Turkish);
}

std::wstring FormatRemainingHhMm(int64_t remainingSeconds, bool showSeconds) {
    int64_t clamped = std::max<int64_t>(0, remainingSeconds);
    wchar_t buf[32];
    if (showSeconds) {
        int h = static_cast<int>(clamped / 3600);
        int m = static_cast<int>((clamped % 3600) / 60);
        int s = static_cast<int>(clamped % 60);
        swprintf_s(buf, L"%02d:%02d:%02d", h, m, s);
    } else {
        int totalMinutes = static_cast<int>((clamped + 59) / 60);
        swprintf_s(buf, L"%02d:%02d", totalMinutes / 60, totalMinutes % 60);
    }
    return std::wstring(buf);
}

FormattedTaskbarDisplay FormatTaskbar(const PrayerCountdownState& state, const AppSettings& settings) {
    AppLanguage eff = ResolveEffectiveLanguage(settings.language);
    bool isRamadan = settings.ramadanIftarSahurMode && (GregorianToHijri(state.calculatedAt.date).month == 9);
    std::wstring vakit = GetEffectivePrayerName(state.nextPrayer, settings);
    std::wstring vakitE = GetEffectivePrayerDativeName(state.nextPrayer, settings);
    std::wstring kisa = GetEffectivePrayerShortCode(state.nextPrayer, settings);
    TimeHM nextHm{state.nextPrayerDateTime.hour, state.nextPrayerDateTime.minute};
    std::wstring saat = nextHm.FormatHHMM();
    std::wstring kalan = FormatRemainingLong(state.remainingSeconds, settings.showSeconds, eff);
    std::wstring kalanKisa = FormatRemainingCompact(state.remainingSeconds, settings.showSeconds, eff);
    std::wstring hhmm = FormatRemainingHhMm(state.remainingSeconds, settings.showSeconds);

    FormattedTaskbarDisplay out;
    out.nextPrayer = state.nextPrayer;
    out.progress = state.progress;
    out.isUrgent = state.isUrgent;

    switch (settings.displayStyle) {
        case TaskbarDisplayStyle::TwoLineClassic:
            out.line1 = FormatUntilPrayerLine(state.nextPrayer, settings, isRamadan);
            out.line2 = kalan;
            out.hasLine2 = true;
            return out;

        case TaskbarDisplayStyle::CompactArrow:
            out.line1 = kisa + L"→" + kalanKisa;
            return out;

        case TaskbarDisplayStyle::CompactArrowSpaced:
            out.line1 = kisa + L" → " + kalan;
            return out;

        case TaskbarDisplayStyle::TwoLineWithTime:
            out.line1 = vakit + L" · " + saat;
            out.line2 = kalan;
            out.hasLine2 = true;
            return out;

        case TaskbarDisplayStyle::IconAndCountdown:
            out.line1 = kalan;
            out.showPhaseIcon = true;
            return out;

        case TaskbarDisplayStyle::TwoLineShort:
            out.line1 = vakit;
            out.line2 = NormalizeTurkishLower(kalanKisa);
            out.hasLine2 = true;
            return out;

        case TaskbarDisplayStyle::MinimalDotTimer:
            out.line1 = settings.showSeconds ? kalan : hhmm;
            out.showDot = true;
            return out;

        case TaskbarDisplayStyle::NextPrayerTime:
            out.line1 = vakit;
            out.line2 = saat;
            out.hasLine2 = true;
            return out;

        case TaskbarDisplayStyle::Custom:
            return FormatCustom(settings.customTemplate, vakit, vakitE, kisa, kalan, kalanKisa, saat, hhmm, state);
    }

    out.line1 = FormatUntilPrayerLine(state.nextPrayer, settings);
    out.line2 = kalan;
    out.hasLine2 = true;
    return out;
}

std::pair<std::wstring, std::wstring> FormatTrayText(const PrayerCountdownState& state, const AppSettings& settings) {
    std::wstring top = GetEffectivePrayerShortCode(state.nextPrayer, settings);
    TimeUnits u = UnitsForLang(settings.language);
    int totalMinutes = std::max(0, static_cast<int>((state.remainingSeconds + 59) / 60));
    std::wstring bottom;
    if (totalMinutes == 0) {
        bottom = L"<1";
    } else if (totalMinutes < 60) {
        bottom = std::to_wstring(totalMinutes);
    } else {
        bottom = std::to_wstring(totalMinutes / 60) + u.trayHrSuffix;
    }
    return {top, bottom};
}

std::pair<std::wstring, std::wstring> FormatTrayText(const PrayerCountdownState& state) {
    AppSettings def;
    return FormatTrayText(state, def);
}

std::wstring FormatTrayTooltip(const PrayerCountdownState& state, const AppSettings& settings) {
    TimeHM nextHm{state.nextPrayerDateTime.hour, state.nextPrayerDateTime.minute};
    std::wstring kalan = FormatRemainingLong(state.remainingSeconds, settings.showSeconds, settings.language);
    std::wstring heading = FormatUntilPrayerLine(state.nextPrayer, settings);

    std::wstring text = settings.cityName + L" · " + heading + L": " +
                        kalan + L" (" + nextHm.FormatHHMM() + L")\n" +
                        GetEffectivePrayerShortCode(PrayerKind::Imsak, settings) + L":" + state.today.imsak.FormatHHMM() + L" " +
                        GetEffectivePrayerShortCode(PrayerKind::Gunes, settings) + L":" + state.today.gunes.FormatHHMM() + L" " +
                        GetEffectivePrayerShortCode(PrayerKind::Ogle, settings) + L":" + state.today.ogle.FormatHHMM() + L" " +
                        GetEffectivePrayerShortCode(PrayerKind::Ikindi, settings) + L":" + state.today.ikindi.FormatHHMM() + L" " +
                        GetEffectivePrayerShortCode(PrayerKind::Aksam, settings) + L":" + state.today.aksam.FormatHHMM() + L" " +
                        GetEffectivePrayerShortCode(PrayerKind::Yatsi, settings) + L":" + state.today.yatsi.FormatHHMM();

    if (text.size() > 127) {
        text.resize(127);
    }
    return text;
}

const wchar_t* GetStyleFriendlyName(TaskbarDisplayStyle style) {
    return GetStyleFriendlyNameLocalized(style, AppLanguage::Turkish);
}

} // namespace vakit

namespace vakit {

std::wstring FormatWidgetDataJson(const PrayerCountdownState& state, const AppSettings& settings) {
    auto q = [](const std::wstring& s) {
        std::wstring out = L"\"";
        for (wchar_t c : s) {
            if (c == L'"' || c == L'\\') { out += L'\\'; out += c; }
            else if (c < 0x20) out += L' ';
            else out += c;
        }
        return out + L"\"";
    };
    TimeHM next{state.nextPrayerDateTime.hour, state.nextPrayerDateTime.minute};
    AppLanguage lang = settings.language;
    // Same hijri source as the flyout header: Diyanet text in Turkish, computed otherwise.
    std::wstring hijri = (ResolveEffectiveLanguage(lang) == AppLanguage::Turkish && !state.today.hijriDateText.empty())
        ? state.today.hijriDateText : FormatHijriDateLocalized(state.today.date, lang);
    std::wstring json = L"{\"heading\":" + q(FormatUntilPrayerLine(state.nextPrayer, settings)) +
                        L",\"remain\":" + q(FormatRemainingLong(state.remainingSeconds, false, lang)) +
                        L",\"next\":" + q(GetEffectivePrayerName(state.nextPrayer, settings)) +
                        L",\"time\":" + q(next.FormatHHMM()) +
                        L",\"city\":" + q(settings.cityName) +
                        L",\"hijri\":" + q(hijri) + L",\"times\":[";
    int nowMin = state.calculatedAt.hour * 60 + state.calculatedAt.minute;
    for (PrayerKind k : kOrderedPrayerKinds) {
        if (k != PrayerKind::Imsak) json += L',';
        TimeHM t = state.today.GetTime(k);
        int diff = t.TotalMinutes() - nowMin;  // minutes from now; negative = already passed today
        std::wstring rel = (diff < 0 ? L"-" : L"+") + FormatRemainingCompact(static_cast<int64_t>(diff < 0 ? -diff : diff) * 60, false, lang);
        json += L"{\"n\":" + q(GetEffectivePrayerName(k, settings)) + L",\"t\":" + q(t.FormatHHMM()) + L",\"d\":" + q(rel) +
                L",\"cur\":" + (k == state.nextPrayer ? L"true" : L"false") +
                L",\"past\":" + (diff < 0 && k != state.nextPrayer ? L"true" : L"false") + L'}';
    }
    return json + L"]}";
}

} // namespace vakit
