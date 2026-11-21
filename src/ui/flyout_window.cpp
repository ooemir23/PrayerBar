#include "flyout_window.h"
#include "../core/prayer_tracking.h"
#include "gdiplus_flat.h"
#include "../core/formatter.h"
#include "../core/i18n.h"
#include "../core/prayer_calc.h"
#include "../platform/audio_player.h"
#include "../platform/theme_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace vakit {

namespace {

constexpr wchar_t kFlyoutClassName[] = L"VakitCubuguFluentFlyout";
constexpr UINT_PTR kTimerCountdownId = 1;
constexpr double kPi = 3.14159265358979323846;

struct FlyoutDipSize {
    int width = 308;
    int height = 356;
};

int FlyoutExtraFooterDip(const AppSettings& settings) {
    return settings.showExtraTimes ? 18 : 0;
}

FlyoutDipSize GetFlyoutDipSize(FlyoutDesignStyle design, const AppSettings& settings) {
    FlyoutDipSize d;
    switch (design) {
        case FlyoutDesignStyle::MinimalGlass:      d = {276, 304}; break;
        case FlyoutDesignStyle::CardGrid:          d = {332, 348}; break;
        case FlyoutDesignStyle::TimelineDashboard: d = {352, 404}; break;
        case FlyoutDesignStyle::CompactFluent:
        default:                                   d = {308, 356}; break;
    }
    d.height += FlyoutExtraFooterDip(settings);
    return d;
}

uint32_t WithAlpha(uint8_t alpha, uint32_t argb) {
    return (static_cast<uint32_t>(alpha) << 24) | (argb & 0x00FFFFFFu);
}

uint32_t PrayerDotArgb(PrayerKind kind, bool passed) {
    uint32_t base = GetPrayerAccentArgb(kind);
    return passed ? WithAlpha(0x78u, base) : base;
}

const wchar_t* KerahatText(KerahatStatus ks, AppLanguage lang) {
    switch (ks) {
        case KerahatStatus::AfterSunrise:  return Tr(lang, StrId::KerahatAfterSunrise);
        case KerahatStatus::BeforeDhuhr:   return Tr(lang, StrId::KerahatBeforeDhuhr);
        case KerahatStatus::BeforeMaghrib: return Tr(lang, StrId::KerahatBeforeMaghrib);
        default: return L"";
    }
}

struct Palette {
    bool isLight = false;
    uint32_t bgSurf = 0;
    uint32_t borderCol = 0;
    uint32_t cardBg = 0;
    uint32_t cardAlt = 0;
    uint32_t heroBg = 0;
    uint32_t heroBorder = 0;
    uint32_t textPrimary = 0;
    uint32_t textSecondary = 0;
    uint32_t accent = 0;
    uint32_t gold = 0;
    uint32_t amberBg = 0;
    uint32_t amberBorder = 0;
    uint32_t rowActiveBg = 0;
    uint32_t chipBg = 0;
    uint32_t btnHoverBg = 0;
};

Palette MakePalette(bool isLight, uint32_t accent) {
    Palette p;
    p.isLight = isLight;
    p.accent = accent;
    p.bgSurf = isLight ? 0xFFF3F3F3u : 0xFF202020u;
    p.borderCol = isLight ? 0x1E000000u : 0x22FFFFFFu;
    p.cardBg = isLight ? 0xFFFFFFFFu : 0xFF2B2B2Bu;
    p.cardAlt = isLight ? 0xFFF9F9FBu : 0xFF262628u;
    p.heroBg = isLight ? WithAlpha(0x18u, accent) : WithAlpha(0x28u, accent);
    p.heroBorder = WithAlpha(0x66u, accent);
    p.textPrimary = isLight ? 0xFF1A1A1Au : 0xFFF5F7FAu;
    p.textSecondary = isLight ? 0xFF5D6472u : 0xFFA6AEBCu;
    p.gold = isLight ? 0xFFB86600u : 0xFFFFCE5Eu;
    p.amberBg = isLight ? 0x22D97706u : 0x2AF59E0Bu;
    p.amberBorder = isLight ? 0x66D97706u : 0x77FBBF24u;
    p.rowActiveBg = WithAlpha(isLight ? 0x1Au : 0x2Eu, accent);
    p.chipBg = isLight ? 0x0F000000u : 0x16FFFFFFu;
    p.btnHoverBg = isLight ? 0x1C000000u : 0x26FFFFFFu;
    return p;
}

std::wstring FormatGregorianSubtext(const LocalDateTime& now, const PrayerCountdownState& state, AppLanguage lang) {
    int mIdx = std::clamp(now.date.month, 1, 12);
    int dIdx = std::clamp(now.date.DayOfWeek(), 0, 6);
    wchar_t gregBuf[96];
    swprintf_s(gregBuf, L"%d %ls %d, %ls",
               now.date.day,
               GetShortMonthName(mIdx, lang),
               now.date.year,
               GetShortWeekdayName(dIdx, lang));
    std::wstring hijri = (ResolveEffectiveLanguage(lang) == AppLanguage::Turkish && !state.today.hijriDateText.empty())
        ? state.today.hijriDateText
        : FormatHijriDateLocalized(now.date, lang);
    return hijri.empty() ? std::wstring(gregBuf) : (std::wstring(gregBuf) + L" · " + hijri);
}

void DrawQiblaCompass(
    void* g,
    void* uiFamily,
    float cx,
    float cy,
    float radius,
    double bearingDeg,
    const Palette& pal,
    AppLanguage lang,
    float s) {

    void* ringPen = nullptr;
    gdip::GdipCreatePen1(pal.borderCol, 1.4f * s, gdip::kUnitPixel, &ringPen);
    if (ringPen) {
        gdip::GdipDrawEllipse(g, ringPen, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        gdip::GdipDeletePen(ringPen);
    }

    // N / E / S / W işaretleri
    const wchar_t* north = GetCompassDirectionLocalized(0.0, lang);
    gdip::DrawTextBox(g, north, uiFamily, 8.0f * s, true, pal.textSecondary,
                      cx - 10.0f * s, cy - radius - 1.0f * s, 20.0f * s, 11.0f * s,
                      gdip::kStringAlignmentCenter, gdip::kStringAlignmentNear);

    double rad = bearingDeg * kPi / 180.0;
    float rNeedle = radius * 0.66f;
    float tipX = cx + static_cast<float>(std::sin(rad)) * rNeedle;
    float tipY = cy - static_cast<float>(std::cos(rad)) * rNeedle;
    float backX = cx - static_cast<float>(std::sin(rad)) * (rNeedle * 0.42f);
    float backY = cy + static_cast<float>(std::cos(rad)) * (rNeedle * 0.42f);

    void* tailPen = nullptr;
    gdip::GdipCreatePen1(WithAlpha(0x70u, pal.textSecondary), 1.6f * s, gdip::kUnitPixel, &tailPen);
    if (tailPen) {
        gdip::GdipDrawLine(g, tailPen, backX, backY, cx, cy);
        gdip::GdipDeletePen(tailPen);
    }

    void* arrowPen = nullptr;
    gdip::GdipCreatePen1(pal.accent, 2.4f * s, gdip::kUnitPixel, &arrowPen);
    if (arrowPen) {
        gdip::GdipDrawLine(g, arrowPen, cx, cy, tipX, tipY);
        gdip::GdipDeletePen(arrowPen);
    }

    void* dotBrush = nullptr;
    gdip::GdipCreateSolidFill(pal.gold, &dotBrush);
    if (dotBrush) {
        float d = 4.5f * s;
        gdip::GdipFillEllipse(g, dotBrush, tipX - d * 0.5f, tipY - d * 0.5f, d, d);
        gdip::GdipDeleteBrush(dotBrush);
    }
    void* centerBrush = nullptr;
    gdip::GdipCreateSolidFill(pal.textPrimary, &centerBrush);
    if (centerBrush) {
        float d = 4.0f * s;
        gdip::GdipFillEllipse(g, centerBrush, cx - d * 0.5f, cy - d * 0.5f, d, d);
        gdip::GdipDeleteBrush(centerBrush);
    }
}

void DrawKerahatChip(
    void* g,
    void* uiFamily,
    float x,
    float y,
    float w,
    float h,
    KerahatStatus ks,
    AppLanguage lang,
    const Palette& pal,
    float s) {

    gdip::FillRoundedRect(g, x, y, w, h, 5.0f * s, pal.amberBg);
    gdip::StrokeRoundedRect(g, x, y, w, h, 5.0f * s, pal.amberBorder, 1.0f);
    std::wstring label = L"⚠ ";
    label += KerahatText(ks, lang);
    gdip::DrawTextBox(g, label, uiFamily, 10.0f * s, true, pal.gold,
                      x + 8.0f * s, y, w - 16.0f * s, h,
                      gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
}

struct FlyoutButtonsOut {
    RECT stopRc{};
    RECT imsakiyeRc{};
    RECT refreshRc{};
    RECT settingsRc{};
    RECT prayerRc[6]{};  // clickable rows for prayer tracking (Güneş stays empty)
};

template <typename H>
void RenderFlyoutSurface(
    void* g,
    int width,
    int height,
    float s,
    const LocalDateTime& now,
    const PrayerCountdownState& state,
    const AppSettings& settings,
    bool isLight,
    H hovered,
    H stopId,
    H imsakiyeId,
    H refreshId,
    H settingsId,
    bool refreshBusy,
    FlyoutButtonsOut& outBtns) {

    gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
    gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAliasGridFit);
    gdip::GdipSetCompositingQuality(g, gdip::kCompositingQualityHighQuality);

    uint32_t accentCol = GetWindowsAccentColorArgb(!isLight, settings.useSystemAccentColor);
    Palette pal = MakePalette(isLight, accentCol);
    AppLanguage lang = settings.language;

    gdip::GdipGraphicsClear(g, pal.bgSurf);

    float w = static_cast<float>(width);
    float h = static_cast<float>(height);
    float pad = 12.0f * s;

    void* uiFamily = gdip::CreateBestUiFontFamily(false);
    void* displayFamily = gdip::CreateBestUiFontFamily(true);
    void* iconFamily = gdip::CreateFluentIconFontFamily();

    gdip::StrokeRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 9.0f * s, pal.borderCol, 1.0f);

    const float footerH = (22.0f + static_cast<float>(FlyoutExtraFooterDip(settings))) * s;
    const bool tracking = settings.prayerTracking;
    // Tracking: remember each farz row as a click target and overlay a check on prayed ones.
    auto trackRow = [&](PrayerKind kind, float rx, float ry, float rw, float rh, float dotCx, float dotCy) {
        if (!tracking || kind == PrayerKind::Gunes) return;
        outBtns.prayerRc[static_cast<int>(kind)] = {static_cast<LONG>(rx), static_cast<LONG>(ry),
                                                    static_cast<LONG>(rx + rw), static_cast<LONG>(ry + rh)};
        bool prayed = tracking::IsPrayed(now.date, kind);
        float r = 7.0f * s;
        void* br = nullptr;
        gdip::GdipCreateSolidFill(prayed ? pal.accent : pal.chipBg, &br);
        if (br) {
            gdip::GdipFillEllipse(g, br, dotCx - r, dotCy - r, r * 2.0f, r * 2.0f);
            gdip::GdipDeleteBrush(br);
        }
        if (prayed) {
            gdip::DrawTextBox(g, L"\u2713", uiFamily, 9.0f * s, true, pal.bgSurf,
                              dotCx - r, dotCy - r, r * 2.0f, r * 2.0f,
                              gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
        }
    };

    // Üst araç çubuğu butonları (sağ üst)
    float btnSz = 26.0f * s;
    float btnGap = 4.0f * s;
    float btnY = 10.0f * s;
    float btnSetX = w - pad - btnSz;
    float btnRefX = btnSetX - btnGap - btnSz;
    float btnImsX = btnRefX - btnGap - btnSz;
    float btnStopX = btnImsX - btnGap - btnSz;

    outBtns.settingsRc = {static_cast<LONG>(btnSetX), static_cast<LONG>(btnY),
                          static_cast<LONG>(btnSetX + btnSz), static_cast<LONG>(btnY + btnSz)};
    outBtns.refreshRc  = {static_cast<LONG>(btnRefX), static_cast<LONG>(btnY),
                          static_cast<LONG>(btnRefX + btnSz), static_cast<LONG>(btnY + btnSz)};
    outBtns.imsakiyeRc = {static_cast<LONG>(btnImsX), static_cast<LONG>(btnY),
                          static_cast<LONG>(btnImsX + btnSz), static_cast<LONG>(btnY + btnSz)};

    bool audioPlaying = AudioPlayer::IsPlaying();
    if (audioPlaying) {
        outBtns.stopRc = {static_cast<LONG>(btnStopX), static_cast<LONG>(btnY),
                          static_cast<LONG>(btnStopX + btnSz), static_cast<LONG>(btnY + btnSz)};
    } else {
        outBtns.stopRc = {};
    }

    auto drawHdrBtn = [&](const RECT& rc, const wchar_t* glyph, const wchar_t* fallback, H targetId, uint32_t fg) {
        uint32_t bg = (hovered == targetId) ? pal.btnHoverBg : pal.chipBg;
        gdip::FillRoundedRect(g, static_cast<float>(rc.left), static_cast<float>(rc.top),
                              btnSz, btnSz, 5.0f * s, bg);
        gdip::DrawFluentIcon(g, iconFamily, glyph, uiFamily, fallback, 12.0f * s, fg,
                             static_cast<float>(rc.left), static_cast<float>(rc.top), btnSz, btnSz);
    };

    if (audioPlaying) {
        drawHdrBtn(outBtns.stopRc, L"\uE71A", L"■", stopId, pal.gold);
    }
    drawHdrBtn(outBtns.imsakiyeRc, L"\uE787", L"▦", imsakiyeId, pal.accent);
    drawHdrBtn(outBtns.refreshRc, refreshBusy ? L"\uE895" : L"\uE72C", refreshBusy ? L"…" : L"↻", refreshId, pal.textPrimary);
    drawHdrBtn(outBtns.settingsRc, L"\uE713", L"⚙", settingsId, pal.textPrimary);

    // Başlık (Şehir + Tarih)
    float headerTextW = (audioPlaying ? btnStopX : btnImsX) - pad - 6.0f * s;
    std::wstring cityTitle = settings.cityName + L", " + settings.countryName;
    gdip::DrawTextBox(g, cityTitle, displayFamily, 13.5f * s, true, pal.textPrimary,
                      pad, 8.0f * s, std::max(60.0f, headerTextW), 18.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

    std::wstring subText = FormatGregorianSubtext(now, state, lang);
    gdip::DrawTextBox(g, subText, uiFamily, 10.2f * s, false, pal.textSecondary,
                      pad, 25.5f * s, std::max(60.0f, headerTextW), 15.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

    bool showKerahat = settings.showKerahatIndicator && state.kerahatStatus != KerahatStatus::None;
    bool isFriday = settings.showKerahatIndicator && (now.date.DayOfWeek() == 5);
    float contentW = w - pad * 2.0f;
    TimeHM nextHm{state.nextPrayerDateTime.hour, state.nextPrayerDateTime.minute};
    TimeHM nowHm{now.hour, now.minute};
    auto entries = state.today.ToOrderedEntries();
    bool isRamadan = settings.ramadanIftarSahurMode && (GregorianToHijri(now.date).month == 9);
    std::wstring nextUpper = FormatHeroHeading(state.nextPrayer, settings, isRamadan);
    std::wstring heroRemain = FormatRemainingLong(state.remainingSeconds, true, lang);
    std::wstring qibla = FormatQiblaLabelLocalized(settings.latitude, settings.longitude, lang);

    if (settings.flyoutDesign == FlyoutDesignStyle::MinimalGlass) {
        // 1: Minimal İnce Liste (Cam)
        gdip::FillRoundedRect(g, 0.0f, 0.0f, w, 2.5f * s, 1.0f, pal.accent);

        float heroY = 46.0f * s;
        gdip::DrawTextBox(g, nextUpper, uiFamily, 9.8f * s, true, pal.accent,
                          pad, heroY, contentW, 14.0f * s,
                          gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, heroRemain, displayFamily, 22.0f * s, true, pal.textPrimary,
                          pad, heroY + 13.0f * s, contentW, 28.0f * s,
                          gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);

        float rowY = heroY + 48.0f * s;
        if (showKerahat) {
            DrawKerahatChip(g, uiFamily, pad, rowY, contentW, 18.0f * s, state.kerahatStatus, lang, pal, s);
            rowY += 22.0f * s;
        }

        float availH = h - footerH - rowY;
        float rowH = availH / 6.0f;
        for (size_t i = 0; i < entries.size(); ++i) {
            PrayerKind kind = entries[i].first;
            TimeHM tm = entries[i].second;
            bool isNext = (kind == state.nextPrayer && state.nextPrayerDateTime.date == state.today.date);
            bool isCurrent = (kind == state.currentPrayer && !isNext);
            bool isPassed = (tm < nowHm && !isCurrent && !isNext);

            if (isNext) {
                gdip::FillRoundedRect(g, pad, rowY, contentW, rowH - 2.0f * s, 4.0f * s, pal.rowActiveBg);
            } else if (i > 0) {
                void* linePen = nullptr;
                gdip::GdipCreatePen1(pal.borderCol, 1.0f, gdip::kUnitPixel, &linePen);
                if (linePen) {
                    gdip::GdipDrawLine(g, linePen, pad + 4.0f * s, rowY, w - pad - 4.0f * s, rowY);
                    gdip::GdipDeletePen(linePen);
                }
            }

            void* dotBrush = nullptr;
            gdip::GdipCreateSolidFill(PrayerDotArgb(kind, isPassed), &dotBrush);
            if (dotBrush) {
                float dSz = 5.5f * s;
                gdip::GdipFillEllipse(g, dotBrush, pad + 6.0f * s, rowY + (rowH - dSz) * 0.5f, dSz, dSz);
                gdip::GdipDeleteBrush(dotBrush);
            }
            trackRow(kind, pad, rowY, contentW, rowH, pad + 8.75f * s, rowY + rowH * 0.5f);

            std::wstring pName = GetEffectivePrayerName(kind, settings);
            if (isFriday && kind == PrayerKind::Ogle) {
                pName += L" · ";
                pName += Tr(lang, StrId::BadgeJumuah);
            }
            uint32_t col = isNext ? pal.accent : (isPassed ? pal.textSecondary : pal.textPrimary);
            gdip::DrawTextBox(g, pName, uiFamily, 11.5f * s, isNext, col,
                              pad + 18.0f * s, rowY, contentW * 0.60f, rowH,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            gdip::DrawTextBox(g, tm.FormatHHMM(), uiFamily, 11.5f * s, isNext || isCurrent, col,
                              pad + contentW - 60.0f * s, rowY, 52.0f * s, rowH,
                              gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);
            rowY += rowH;
        }
    } else if (settings.flyoutDesign == FlyoutDesignStyle::CardGrid) {
        // 2: Modern 2x3 Bento Kartları
        float heroY = 46.0f * s;
        float heroH = 74.0f * s;
        gdip::FillRoundedRect(g, pad, heroY, contentW, heroH, 8.0f * s, pal.heroBg);
        gdip::StrokeRoundedRect(g, pad, heroY, contentW, heroH, 8.0f * s, pal.heroBorder, 1.0f);

        // Sol dairesel ilerleme halkası
        float ringSz = 48.0f * s;
        float rx = pad + 12.0f * s;
        float ry = heroY + (heroH - ringSz) * 0.5f;
        void* trackPen = nullptr;
        gdip::GdipCreatePen1(pal.chipBg, 4.0f * s, gdip::kUnitPixel, &trackPen);
        if (trackPen) {
            gdip::GdipDrawEllipse(g, trackPen, rx, ry, ringSz, ringSz);
            gdip::GdipDeletePen(trackPen);
        }
        void* arcPen = nullptr;
        gdip::GdipCreatePen1(pal.accent, 4.0f * s, gdip::kUnitPixel, &arcPen);
        if (arcPen) {
            float sweep = std::max(14.0f, static_cast<float>(360.0 * std::clamp(state.progress, 0.03, 1.0)));
            gdip::GdipDrawArc(g, arcPen, rx, ry, ringSz, ringSz, -90.0f, sweep);
            gdip::GdipDeletePen(arcPen);
        }
        int pct = static_cast<int>(std::llround(std::clamp(state.progress, 0.0, 1.0) * 100.0));
        wchar_t pctBuf[16];
        swprintf_s(pctBuf, L"%d%%", pct);
        gdip::DrawTextBox(g, pctBuf, uiFamily, 11.0f * s, true, pal.textPrimary,
                          rx, ry, ringSz, ringSz, gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);

        float textX = rx + ringSz + 12.0f * s;
        float textW = w - pad - textX - 8.0f * s;
        gdip::DrawTextBox(g, nextUpper, uiFamily, 9.8f * s, true, pal.accent,
                          textX, heroY + 8.0f * s, textW, 14.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, heroRemain, displayFamily, 19.0f * s, true, pal.textPrimary,
                          textX, heroY + 23.0f * s, textW, 25.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, nextHm.FormatHHMM() + L" · " + qibla, uiFamily, 9.8f * s, false, pal.textSecondary,
                          textX, heroY + 49.0f * s, textW, 15.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

        float gridY = heroY + heroH + 8.0f * s;
        if (showKerahat) {
            DrawKerahatChip(g, uiFamily, pad, gridY, contentW, 20.0f * s, state.kerahatStatus, lang, pal, s);
            gridY += 24.0f * s;
        }

        float gap = 6.0f * s;
        float cardW = (contentW - gap) * 0.5f;
        float availH = h - footerH - gridY;
        float cardH = (availH - gap * 2.0f) / 3.0f;

        for (size_t i = 0; i < entries.size(); ++i) {
            int col = static_cast<int>(i % 2);
            int row = static_cast<int>(i / 2);
            float cx = pad + col * (cardW + gap);
            float cy = gridY + row * (cardH + gap);

            PrayerKind kind = entries[i].first;
            TimeHM tm = entries[i].second;
            bool isNext = (kind == state.nextPrayer && state.nextPrayerDateTime.date == state.today.date);
            bool isCurrent = (kind == state.currentPrayer && !isNext);
            bool isPassed = (tm < nowHm && !isCurrent && !isNext);

            gdip::FillRoundedRect(g, cx, cy, cardW, cardH, 7.0f * s, isNext ? pal.rowActiveBg : pal.cardBg);
            gdip::StrokeRoundedRect(g, cx, cy, cardW, cardH, 7.0f * s, isNext ? pal.accent : pal.borderCol, isNext ? 1.3f : 1.0f);

            void* dotBrush = nullptr;
            gdip::GdipCreateSolidFill(PrayerDotArgb(kind, isPassed), &dotBrush);
            if (dotBrush) {
                float dSz = 6.5f * s;
                gdip::GdipFillEllipse(g, dotBrush, cx + 10.0f * s, cy + 12.0f * s, dSz, dSz);
                gdip::GdipDeleteBrush(dotBrush);
            }
            trackRow(kind, cx, cy, cardW, cardH, cx + 13.25f * s, cy + 15.25f * s);

            uint32_t textCol = isPassed ? pal.textSecondary : pal.textPrimary;
            gdip::DrawTextBox(g, GetEffectivePrayerName(kind, settings), uiFamily, 11.2f * s, isNext, textCol,
                              cx + 21.0f * s, cy + 7.0f * s, cardW - 30.0f * s, 16.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            if (isNext || isCurrent || (isFriday && kind == PrayerKind::Ogle)) {
                const wchar_t* tag = isNext ? Tr(lang, StrId::BadgeNext)
                                            : (isCurrent ? Tr(lang, StrId::BadgeCurrent) : Tr(lang, StrId::BadgeJumuah));
                gdip::DrawTextBox(g, tag, uiFamily, 9.2f * s, true, isNext ? pal.accent : pal.gold,
                                  cx + cardW - 58.0f * s, cy + 7.0f * s, 50.0f * s, 15.0f * s,
                                  gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);
            }

            gdip::DrawTextBox(g, tm.FormatHHMM(), displayFamily, 16.0f * s, true, textCol,
                              cx + 10.0f * s, cy + cardH - 26.0f * s, cardW - 20.0f * s, 21.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        }
    } else if (settings.flyoutDesign == FlyoutDesignStyle::TimelineDashboard) {
        // 3: Zaman Çizelgesi + Kıble Pusulası
        float heroY = 46.0f * s;
        float heroH = 82.0f * s;
        gdip::FillRoundedRect(g, pad, heroY, contentW, heroH, 8.0f * s, pal.heroBg);
        gdip::StrokeRoundedRect(g, pad, heroY, contentW, heroH, 8.0f * s, pal.heroBorder, 1.0f);

        float dialR = 25.0f * s;
        float dialCx = w - pad - dialR - 14.0f * s;
        float dialCy = heroY + 34.0f * s;
        double bearing = CalculateQiblaBearing(settings.latitude, settings.longitude);
        DrawQiblaCompass(g, uiFamily, dialCx, dialCy, dialR, bearing, pal, lang, s);
        gdip::DrawTextBox(g, qibla, uiFamily, 9.2f * s, true, pal.textSecondary,
                          dialCx - 42.0f * s, heroY + heroH - 18.0f * s, 84.0f * s, 14.0f * s,
                          gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);

        float leftW = dialCx - dialR - pad - 14.0f * s;
        gdip::DrawTextBox(g, nextUpper, uiFamily, 10.0f * s, true, pal.accent,
                          pad + 10.0f * s, heroY + 7.0f * s, leftW, 15.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, heroRemain, displayFamily, 20.0f * s, true, pal.textPrimary,
                          pad + 10.0f * s, heroY + 23.0f * s, leftW, 28.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, std::wstring(Tr(lang, StrId::PrayerTimeHeader)) + L": " + nextHm.FormatHHMM(),
                          uiFamily, 10.5f * s, true, pal.gold,
                          pad + 10.0f * s, heroY + 54.0f * s, leftW, 16.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

        // Yatay 6 düğümlü zaman çizelgesi
        float tlY = heroY + heroH + 8.0f * s;
        float tlH = 34.0f * s;
        gdip::FillRoundedRect(g, pad, tlY, contentW, tlH, 6.0f * s, pal.cardBg);
        float nodeStartX = pad + 22.0f * s;
        float nodeEndX = w - pad - 22.0f * s;
        float lineY = tlY + 12.0f * s;
        void* trackPen = nullptr;
        gdip::GdipCreatePen1(pal.borderCol, 2.0f * s, gdip::kUnitPixel, &trackPen);
        if (trackPen) {
            gdip::GdipDrawLine(g, trackPen, nodeStartX, lineY, nodeEndX, lineY);
            gdip::GdipDeletePen(trackPen);
        }
        for (size_t i = 0; i < 6; ++i) {
            float nx = nodeStartX + (nodeEndX - nodeStartX) * (static_cast<float>(i) / 5.0f);
            PrayerKind kind = entries[i].first;
            bool isNext = (kind == state.nextPrayer && state.nextPrayerDateTime.date == state.today.date);
            bool isPassed = (entries[i].second < nowHm && !isNext && kind != state.currentPrayer);
            float nd = isNext ? 9.5f * s : 6.5f * s;
            void* nb = nullptr;
            gdip::GdipCreateSolidFill(isNext ? pal.accent : PrayerDotArgb(kind, isPassed), &nb);
            if (nb) {
                gdip::GdipFillEllipse(g, nb, nx - nd * 0.5f, lineY - nd * 0.5f, nd, nd);
                gdip::GdipDeleteBrush(nb);
            }
            gdip::DrawTextBox(g, GetEffectivePrayerShortCode(kind, settings), uiFamily, 8.8f * s, isNext,
                              isNext ? pal.accent : pal.textSecondary,
                              nx - 16.0f * s, tlY + 18.0f * s, 32.0f * s, 13.0f * s,
                              gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
        }

        float listY = tlY + tlH + 7.0f * s;
        if (showKerahat) {
            DrawKerahatChip(g, uiFamily, pad, listY, contentW, 19.0f * s, state.kerahatStatus, lang, pal, s);
            listY += 23.0f * s;
        }

        float availH = h - footerH - listY;
        float gap = 3.0f * s;
        float rowH = (availH - gap * 5.0f) / 6.0f;
        int nowMin = now.hour * 60 + now.minute;
        for (size_t i = 0; i < entries.size(); ++i) {
            PrayerKind kind = entries[i].first;
            TimeHM tm = entries[i].second;
            bool isNext = (kind == state.nextPrayer && state.nextPrayerDateTime.date == state.today.date);
            bool isCurrent = (kind == state.currentPrayer && !isNext);
            bool isPassed = (tm < nowHm && !isCurrent && !isNext);

            gdip::FillRoundedRect(g, pad, listY, contentW, rowH, 6.0f * s, isNext ? pal.rowActiveBg : pal.cardBg);
            if (isNext) {
                gdip::StrokeRoundedRect(g, pad, listY, contentW, rowH, 6.0f * s, pal.accent, 1.0f);
            }

            void* dotBrush = nullptr;
            gdip::GdipCreateSolidFill(PrayerDotArgb(kind, isPassed), &dotBrush);
            if (dotBrush) {
                float dSz = 6.5f * s;
                gdip::GdipFillEllipse(g, dotBrush, pad + 9.0f * s, listY + (rowH - dSz) * 0.5f, dSz, dSz);
                gdip::GdipDeleteBrush(dotBrush);
            }
            trackRow(kind, pad, listY, contentW, rowH, pad + 12.25f * s, listY + rowH * 0.5f);

            uint32_t textCol = isPassed ? pal.textSecondary : pal.textPrimary;
            std::wstring pName = GetEffectivePrayerName(kind, settings);
            if (isFriday && kind == PrayerKind::Ogle) {
                pName += L" · ";
                pName += Tr(lang, StrId::BadgeJumuah);
            }
            gdip::DrawTextBox(g, pName, uiFamily, 11.5f * s, isNext, textCol,
                              pad + 22.0f * s, listY, 125.0f * s, rowH,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            int diffMin = tm.TotalMinutes() - nowMin;
            wchar_t diffBuf[32]{};
            int absD = std::abs(diffMin);
            if (diffMin >= 0) {
                swprintf_s(diffBuf, absD >= 60 ? L"+%dh %dm" : L"+%dm", absD >= 60 ? absD / 60 : absD, absD % 60);
            } else {
                swprintf_s(diffBuf, absD >= 60 ? L"-%dh %dm" : L"-%dm", absD >= 60 ? absD / 60 : absD, absD % 60);
            }
            if (absD < 60) {
                swprintf_s(diffBuf, diffMin >= 0 ? L"+%dm" : L"-%dm", absD);
            }
            gdip::DrawTextBox(g, diffBuf, uiFamily, 9.8f * s, isNext, isNext ? pal.accent : pal.textSecondary,
                              pad + contentW - 118.0f * s, listY, 58.0f * s, rowH,
                              gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);
            gdip::DrawTextBox(g, tm.FormatHHMM(), uiFamily, 12.0f * s, true, textCol,
                              pad + contentW - 56.0f * s, listY, 47.0f * s, rowH,
                              gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);
            listY += rowH + gap;
        }
    } else {
        // 0: CompactFluent (Windows 11 Klasik)
        float heroY = 47.0f * s;
        float heroH = 68.0f * s;
        gdip::FillRoundedRect(g, pad, heroY, contentW, heroH, 8.0f * s, pal.heroBg);
        gdip::StrokeRoundedRect(g, pad, heroY, contentW, heroH, 8.0f * s, pal.heroBorder, 1.0f);

        gdip::DrawTextBox(g, nextUpper, uiFamily, 10.0f * s, true, pal.accent,
                          pad + 10.0f * s, heroY + 6.0f * s, contentW * 0.56f, 15.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, qibla, uiFamily, 9.8f * s, false, pal.textSecondary,
                          pad + contentW * 0.50f, heroY + 6.0f * s, contentW * 0.50f - 10.0f * s, 15.0f * s,
                          gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);

        gdip::DrawTextBox(g, heroRemain, displayFamily, 20.0f * s, true, pal.textPrimary,
                          pad + 10.0f * s, heroY + 23.0f * s, contentW * 0.72f, 26.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, nextHm.FormatHHMM(), displayFamily, 15.0f * s, true, pal.gold,
                          pad + contentW * 0.65f, heroY + 24.0f * s, contentW * 0.35f - 10.0f * s, 24.0f * s,
                          gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);

        float barX = pad + 10.0f * s;
        float barY = heroY + heroH - 10.0f * s;
        float barW = contentW - 20.0f * s;
        float barH = 3.5f * s;
        gdip::FillRoundedRect(g, barX, barY, barW, barH, 2.0f * s, pal.chipBg);
        gdip::FillRoundedRect(g, barX, barY,
                              std::max(8.0f * s, barW * static_cast<float>(std::clamp(state.progress, 0.02, 1.0))),
                              barH, 2.0f * s, pal.accent);

        float rowY = heroY + heroH + 8.0f * s;
        if (showKerahat) {
            DrawKerahatChip(g, uiFamily, pad, rowY, contentW, 20.0f * s, state.kerahatStatus, lang, pal, s);
            rowY += 24.0f * s;
        }

        float availH = h - footerH - rowY;
        float rowGap = 3.5f * s;
        float rowH = (availH - rowGap * 5.0f) / 6.0f;

        for (size_t i = 0; i < entries.size(); ++i) {
            PrayerKind kind = entries[i].first;
            TimeHM tm = entries[i].second;
            bool isNext = (kind == state.nextPrayer && state.nextPrayerDateTime.date == state.today.date);
            bool isCurrent = (kind == state.currentPrayer && !isNext);
            bool isPassed = (tm < nowHm && !isCurrent && !isNext);

            uint32_t rBg = isNext ? pal.rowActiveBg : pal.cardBg;
            gdip::FillRoundedRect(g, pad, rowY, contentW, rowH, 6.0f * s, rBg);
            if (isNext) {
                gdip::StrokeRoundedRect(g, pad, rowY, contentW, rowH, 6.0f * s, pal.accent, 1.0f);
            }

            void* dotBrush = nullptr;
            gdip::GdipCreateSolidFill(PrayerDotArgb(kind, isPassed), &dotBrush);
            if (dotBrush) {
                float dSz = 6.5f * s;
                gdip::GdipFillEllipse(g, dotBrush, pad + 9.0f * s, rowY + (rowH - dSz) * 0.5f, dSz, dSz);
                gdip::GdipDeleteBrush(dotBrush);
            }
            trackRow(kind, pad, rowY, contentW, rowH, pad + 12.25f * s, rowY + rowH * 0.5f);

            uint32_t rTextCol = isPassed ? pal.textSecondary : pal.textPrimary;
            gdip::DrawTextBox(g, GetEffectivePrayerName(kind, settings), uiFamily, 12.0f * s, isNext, rTextCol,
                              pad + 22.0f * s, rowY, 108.0f * s, rowH,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            if (isNext || isCurrent || (isFriday && kind == PrayerKind::Ogle)) {
                const wchar_t* badge = isNext ? Tr(lang, StrId::BadgeNext)
                                              : (isCurrent ? Tr(lang, StrId::BadgeCurrent) : Tr(lang, StrId::BadgeJumuah));
                float bW = isNext ? 54.0f * s : 46.0f * s;
                float bH = 16.0f * s;
                float bX = pad + contentW - 56.0f * s - bW;
                float bY = rowY + (rowH - bH) * 0.5f;
                gdip::FillRoundedRect(g, bX, bY, bW, bH, 4.0f * s, pal.chipBg);
                gdip::DrawTextBox(g, badge, uiFamily, 9.2f * s, true, isNext ? pal.accent : pal.gold,
                                  bX, bY, bW, bH, gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
            }

            gdip::DrawTextBox(g, tm.FormatHHMM(), uiFamily, 12.2f * s, true, rTextCol,
                              pad + contentW - 54.0f * s, rowY, 45.0f * s, rowH,
                              gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);
            rowY += rowH + rowGap;
        }
    }

    // Sade 1 satır alt bilgi
    std::wstring srcText = (state.today.source == L"Yerel Hesaplama")
        ? std::wstring(Tr(lang, StrId::SourceLocalCalc))
        : state.today.source;
    std::wstring footer = std::wstring(Tr(lang, StrId::SourcePrefix)) + srcText;
    if (settings.showExtraTimes) {
        ExtraTimes x = ComputeExtraTimes(state.today, state.tomorrowImsak);
        std::wstring extra = std::wstring(Tr(lang, StrId::ExtraDuha)) + L" " + x.duha.FormatHHMM() + L"  ·  " +
                             Tr(lang, StrId::ExtraMidnight) + L" " + x.midnight.FormatHHMM() + L"  ·  " +
                             Tr(lang, StrId::ExtraTahajjud) + L" " + x.tahajjud.FormatHHMM();
        gdip::DrawTextBox(g, extra, uiFamily, 9.8f * s, true, pal.textPrimary,
                          pad, h - footerH + 2.0f * s, contentW, 15.0f * s,
                          gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
    }
    float sourceW = contentW;
    if (tracking) {
        wchar_t cnt[16];
        swprintf_s(cnt, L"%d/35", tracking::CountPrayedInLastDays(now.date, 7));
        wchar_t weekBuf[96];
        swprintf_s(weekBuf, Tr(lang, StrId::TrackWeek), cnt);
        gdip::DrawTextBox(g, weekBuf, uiFamily, 9.8f * s, true, pal.accent,
                          pad + contentW * 0.5f, h - 20.0f * s, contentW * 0.5f, 15.0f * s,
                          gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);
        sourceW = contentW * 0.5f;
    }
    gdip::DrawTextBox(g, footer, uiFamily, 9.8f * s, false, pal.textSecondary,
                      pad, h - 20.0f * s, sourceW, 15.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

    if (iconFamily) gdip::GdipDeleteFontFamily(iconFamily);
    if (displayFamily) gdip::GdipDeleteFontFamily(displayFamily);
    if (uiFamily) gdip::GdipDeleteFontFamily(uiFamily);
}

} // namespace

FlyoutWindow::FlyoutWindow(
    SettingsStore& settingsStore,
    ScheduleService& scheduleService,
    std::function<void()> onOpenSettings,
    std::function<void()> onOpenImsakiye,
    std::function<void(bool)> onVisibilityChanged,
    std::function<void()> onForceRefresh)
    : settingsStore_(settingsStore),
      scheduleService_(scheduleService),
      onOpenSettings_(std::move(onOpenSettings)),
      onOpenImsakiye_(std::move(onOpenImsakiye)),
      onVisibilityChanged_(std::move(onVisibilityChanged)),
      onForceRefresh_(std::move(onForceRefresh)) {

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kFlyoutClassName;
    RegisterClassExW(&wc);
}

FlyoutWindow::~FlyoutWindow() {
    CloseNow();
}

void FlyoutWindow::ShowRightDocked(RECT anchorRect) {
    if (hwnd_) {
        CloseNow();
        return;
    }

    closing_ = false;
    hovered_ = HoverTarget::None;
    trackingLeave_ = false;

    settings_ = settingsStore_.Load();
    const AppSettings& settings = settings_;
    autoOptimizeMemory_ = settings.autoOptimizeMemory;
    openedDesign_ = settings.flyoutDesign;

    HWND taskbarHwnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    UINT dpi = taskbarHwnd ? GetDpiForWindowCompat(taskbarHwnd) : 96;
    scale_ = dpi >= 96 ? static_cast<double>(dpi) / 96.0 : 1.0;

    FlyoutDipSize dip = GetFlyoutDipSize(openedDesign_, settings);
    int winW = static_cast<int>(std::llround(dip.width * scale_));
    int winH = static_cast<int>(std::llround(dip.height * scale_));

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kFlyoutClassName,
        L"PrayerBar",
        WS_POPUP,
        0, 0, winW, winH,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return;

    bool isLight = IsAppLight(settings.themeMode);
    ApplyWindowDwmStyle(hwnd_, !isLight, 2 /*DWMWCP_ROUND*/, 3 /*DWMSBT_TRANSIENTWINDOW*/);

    PositionRightDocked(anchorRect, winW, winH);
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    SetTimer(hwnd_, kTimerCountdownId, 1000, nullptr);
    if (onVisibilityChanged_) onVisibilityChanged_(true);
}

void FlyoutWindow::CloseNow() {
    if (!hwnd_) return;
    if (closing_) return;
    closing_ = true;
    KillTimer(hwnd_, kTimerCountdownId);
    HWND h = hwnd_;
    hwnd_ = nullptr;
    DestroyWindow(h);
    lastClosedTick_ = GetTickCount64();
    if (onVisibilityChanged_) onVisibilityChanged_(false);
    if (autoOptimizeMemory_) {
        TrimWorkingSet();
    }
}

void FlyoutWindow::RefreshView() {
    if (!hwnd_) return;
    int oldExtra = FlyoutExtraFooterDip(settings_);
    settings_ = settingsStore_.Load();
    const AppSettings& settings = settings_;
    autoOptimizeMemory_ = settings.autoOptimizeMemory;
    if (settings.flyoutDesign != openedDesign_ || FlyoutExtraFooterDip(settings) != oldExtra) {
        openedDesign_ = settings.flyoutDesign;
        FlyoutDipSize dip = GetFlyoutDipSize(openedDesign_, settings);
        int winW = static_cast<int>(std::llround(dip.width * scale_));
        int winH = static_cast<int>(std::llround(dip.height * scale_));
        PositionRightDocked({}, winW, winH);
    }
    bool isLight = IsAppLight(settings.themeMode);
    ApplyWindowDwmStyle(hwnd_, !isLight, 2, 3);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FlyoutWindow::PositionRightDocked(RECT anchorRect, int winW, int winH) {
    HWND taskbarHwnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    HMONITOR mon = taskbarHwnd
        ? MonitorFromWindow(taskbarHwnd, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint({anchorRect.right, anchorRect.bottom}, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
        mi.rcMonitor = mi.rcWork;
    }

    int padX = static_cast<int>(std::llround(12.0 * scale_));
    int padY = static_cast<int>(std::llround(10.0 * scale_));

    int x = mi.rcWork.right - winW - padX;
    if (x < mi.rcWork.left + 8) x = mi.rcWork.left + 8;

    int y = mi.rcWork.bottom - winH - padY;
    RECT tbRect{};
    if (taskbarHwnd && GetWindowRect(taskbarHwnd, &tbRect)) {
        if (tbRect.top < (mi.rcMonitor.top + mi.rcMonitor.bottom) / 2) {
            y = mi.rcWork.top + padY;
        }
    }
    if (y < mi.rcWork.top + 8) y = mi.rcWork.top + 8;

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, winW, winH, SWP_NOACTIVATE);
}

FlyoutWindow::HoverTarget FlyoutWindow::HitTestButton(int x, int y) const {
    POINT pt{x, y};
    if (!IsRectEmpty(&btnStopRc_) && PtInRect(&btnStopRc_, pt)) return HoverTarget::StopAudio;
    if (PtInRect(&btnImsakiyeRc_, pt)) return HoverTarget::Imsakiye;
    if (PtInRect(&btnRefreshRc_, pt)) return HoverTarget::Refresh;
    if (PtInRect(&btnSettingsRc_, pt)) return HoverTarget::Settings;
    return HoverTarget::None;
}

void FlyoutWindow::UpdateButtonsHover(int x, int y) {
    HoverTarget hit = HitTestButton(x, y);
    if (hit != hovered_) {
        hovered_ = hit;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void FlyoutWindow::PaintClient(HDC hdc, int width, int height) {
    gdip::EnsureInitialized();

    HDC memDc = CreateCompatibleDC(hdc);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp || !bits) {
        if (hBmp) DeleteObject(hBmp);
        DeleteDC(memDc);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(memDc, hBmp);
    void* gdiBmp = nullptr;
    void* g = nullptr;

    if (gdip::GdipCreateBitmapFromScan0(width, height, width * 4, gdip::kPixelFormat32bppPARGB, bits, &gdiBmp) == 0 && gdiBmp) {
        if (gdip::GdipGetImageGraphicsContext(gdiBmp, &g) == 0 && g) {
            const AppSettings& settings = settings_;
            bool isLight = IsAppLight(settings.themeMode);
            LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
            PrayerCountdownState state = scheduleService_.ComputeState(now, settings);

            FlyoutButtonsOut btns{};
            RenderFlyoutSurface(
                g, width, height, static_cast<float>(scale_), now, state, settings, isLight,
                hovered_, HoverTarget::StopAudio, HoverTarget::Imsakiye, HoverTarget::Refresh, HoverTarget::Settings,
                refreshBusy_, btns);

            btnStopRc_ = btns.stopRc;
            btnImsakiyeRc_ = btns.imsakiyeRc;
            btnRefreshRc_ = btns.refreshRc;
            btnSettingsRc_ = btns.settingsRc;
            for (int i = 0; i < 6; ++i) prayerRc_[i] = btns.prayerRc[i];
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gdiBmp);
    }

    BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDc);
}

LRESULT CALLBACK FlyoutWindow::WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FlyoutWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<FlyoutWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<FlyoutWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    return self ? self->WndProc(hWnd, msg, wParam, lParam) : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT FlyoutWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc{};
            GetClientRect(hWnd, &rc);
            PaintClient(hdc, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_TIMER:
            if (wParam == kTimerCountdownId) {
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;

        case WM_MOUSEMOVE: {
            if (!trackingLeave_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hWnd, 0};
                TrackMouseEvent(&tme);
                trackingLeave_ = true;
            }
            int x = static_cast<short>(LOWORD(lParam));
            int y = static_cast<short>(HIWORD(lParam));
            UpdateButtonsHover(x, y);
            return 0;
        }

        case WM_MOUSELEAVE:
            trackingLeave_ = false;
            if (hovered_ != HoverTarget::None) {
                hovered_ = HoverTarget::None;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONUP: {
            int x = static_cast<short>(LOWORD(lParam));
            int y = static_cast<short>(HIWORD(lParam));
            HoverTarget hit = HitTestButton(x, y);
            if (hit == HoverTarget::StopAudio) {
                AudioPlayer::Stop();
                InvalidateRect(hWnd, nullptr, FALSE);
            } else if (hit == HoverTarget::Imsakiye) {
                CloseNow();
                if (onOpenImsakiye_) onOpenImsakiye_();
            } else if (hit == HoverTarget::Refresh) {
                if (!refreshBusy_ && onForceRefresh_) {
                    refreshBusy_ = true;
                    InvalidateRect(hWnd, nullptr, FALSE);
                    onForceRefresh_();
                    refreshBusy_ = false;
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
            } else if (hit == HoverTarget::Settings) {
                CloseNow();
                if (onOpenSettings_) onOpenSettings_();
            } else if (settings_.prayerTracking) {
                POINT pt{x, y};
                for (int i = 0; i < 6; ++i) {
                    if (!IsRectEmpty(&prayerRc_[i]) && PtInRect(&prayerRc_[i], pt)) {
                        tracking::TogglePrayed(ScheduleService::GetCurrentLocalDateTime().date, static_cast<PrayerKind>(i));
                        InvalidateRect(hWnd, nullptr, FALSE);
                        break;
                    }
                }
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                CloseNow();
                return 0;
            }
            break;

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE && !closing_) {
                CloseNow();
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hWnd, kTimerCountdownId);
            hwnd_ = nullptr;
            return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

PillRenderResult SmokeTestFlyoutRender(
    FlyoutDesignStyle design,
    const PrayerCountdownState& state,
    const AppSettings& settings,
    double scale,
    std::vector<uint32_t>* outPixels,
    int* outWidth,
    int* outHeight) {

    gdip::EnsureInitialized();
    AppSettings copy = settings;
    copy.flyoutDesign = design;
    FlyoutDipSize dip = GetFlyoutDipSize(design, copy);
    int width = static_cast<int>(std::llround(dip.width * scale));
    int height = static_cast<int>(std::llround(dip.height * scale));

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return {-1, 0};

    HDC memDc = CreateCompatibleDC(screenDc);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    PillRenderResult res;

    if (hBmp && bits) {
        HGDIOBJ oldBmp = SelectObject(memDc, hBmp);
        void* gdiBmp = nullptr;
        void* g = nullptr;
        int st = gdip::GdipCreateBitmapFromScan0(width, height, width * 4, gdip::kPixelFormat32bppPARGB, bits, &gdiBmp);
        if (st == 0 && gdiBmp) {
            st = gdip::GdipGetImageGraphicsContext(gdiBmp, &g);
            if (st == 0 && g) {
                FlyoutButtonsOut btns{};
                RenderFlyoutSurface(
                    g, width, height, static_cast<float>(scale), state.calculatedAt, state, copy, false,
                    0, 1, 2, 3, 4, false, btns);
                const auto* px = static_cast<const uint32_t*>(bits);
                int total = width * height;
                int nonZero = 0;
                for (int i = 0; i < total; ++i) {
                    if ((px[i] & 0xFF000000u) != 0) ++nonZero;
                }
                res.status = 0;
                res.nonZeroPixels = nonZero;
                if (outPixels) outPixels->assign(px, px + total);
                if (outWidth) *outWidth = width;
                if (outHeight) *outHeight = height;
                gdip::GdipDeleteGraphics(g);
            } else {
                res.status = st == 0 ? -4 : st;
            }
            gdip::GdipDisposeImage(gdiBmp);
        } else {
            res.status = st == 0 ? -3 : st;
        }
        SelectObject(memDc, oldBmp);
        DeleteObject(hBmp);
    } else {
        res.status = -2;
    }

    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
    return res;
}

} // namespace vakit
