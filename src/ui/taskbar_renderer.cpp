#include "taskbar_renderer.h"
#include "gdiplus_flat.h"
#include "theme_palette.h"
#include "../platform/audio_player.h"
#include "../platform/theme_utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace vakit {

namespace {

float GetFontScaleMultiplier(FontSizePreset preset) {
    switch (preset) {
        case FontSizePreset::Small: return 0.88f;
        case FontSizePreset::Large: return 1.14f;
        default: return 1.0f;
    }
}

float MeasureTextWidth(void* g, const std::wstring& text, void* font, void* strFormat) {
    if (text.empty()) return 0.0f;
    gdip::RectF layout{0.0f, 0.0f, 1000.0f, 100.0f};
    gdip::RectF bounds{};
    gdip::GdipMeasureString(g, text.c_str(), static_cast<int>(text.size()), font, &layout, strFormat, &bounds, nullptr, nullptr);
    return bounds.width;
}

void DrawPhaseGlyph(void* g, float x, float y, float size, PrayerKind kind, uint32_t color) {
    void* brush = nullptr;
    void* pen = nullptr;
    if (kind == PrayerKind::Imsak || kind == PrayerKind::Yatsi) {
        gdip::GdipCreateSolidFill(color, &brush);
        gdip::GdipCreatePen1(color, std::max(1.4f, size * 0.14f), gdip::kUnitPixel, &pen);
        float inset = size * 0.12f;
        gdip::GdipDrawArc(g, pen, x + inset, y + inset, size - inset * 2.0f, size - inset * 2.0f, 45.0f, 250.0f);
        gdip::GdipFillEllipse(g, brush, x + size * 0.62f, y + size * 0.22f, size * 0.18f, size * 0.18f);
    } else if (kind == PrayerKind::Gunes || kind == PrayerKind::Aksam) {
        gdip::GdipCreatePen1(color, std::max(1.3f, size * 0.12f), gdip::kUnitPixel, &pen);
        float hy = y + size * 0.68f;
        gdip::GdipDrawLine(g, pen, x + size * 0.10f, hy, x + size * 0.90f, hy);
        gdip::GdipDrawArc(g, pen, x + size * 0.22f, y + size * 0.28f, size * 0.56f, size * 0.56f, 180.0f, 180.0f);
    } else {
        gdip::GdipCreateSolidFill(color, &brush);
        gdip::GdipCreatePen1(color, std::max(1.2f, size * 0.11f), gdip::kUnitPixel, &pen);
        float core = size * 0.44f;
        float offset = (size - core) * 0.5f;
        gdip::GdipFillEllipse(g, brush, x + offset, y + offset, core, core);
        float cx = x + size * 0.5f;
        float cy = y + size * 0.5f;
        gdip::GdipDrawLine(g, pen, cx, y + size * 0.05f, cx, y + size * 0.20f);
        gdip::GdipDrawLine(g, pen, cx, y + size * 0.80f, cx, y + size * 0.95f);
        gdip::GdipDrawLine(g, pen, x + size * 0.05f, cy, x + size * 0.20f, cy);
        gdip::GdipDrawLine(g, pen, x + size * 0.80f, cy, x + size * 0.95f, cy);
    }
    if (pen) gdip::GdipDeletePen(pen);
    if (brush) gdip::GdipDeleteBrush(brush);
}

void DrawPillSurface(
    void* g,
    int width,
    int height,
    float s,
    const FormattedTaskbarDisplay& display,
    const AppSettings& settings,
    bool activeHighlight,
    bool isLight,
    bool useOpaqueBackdrop) {

    gdip::GdipGraphicsClear(g, 0x00000000u);
    gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
    gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAlias);
    gdip::GdipSetCompositingQuality(g, gdip::kCompositingQualityHighQuality);

    auto pal = theme::GetUiPalette(!isLight, settings.useSystemAccentColor);
    float radius = 6.0f * s;
    uint32_t bgColor = 0;
    uint32_t borderColor = 0;
    bool drawFrame = true;

    uint32_t primaryColor = 0;
    uint32_t secondaryColor = 0;
    uint32_t singleLineColor = 0;
    uint32_t accentColor = display.isUrgent ? (isLight ? 0xFFD97706u : pal.gold) : pal.accent;

    if (!settings.showTaskbarFrame) {
        drawFrame = false;
        if (isLight) {
            bgColor = display.isUrgent
                ? 0xF0FEF3C7u
                : (activeHighlight ? 0x22000000u : (useOpaqueBackdrop ? 0xF2FFFFFFu : 0x01000000u));
            primaryColor = display.isUrgent ? 0xFFB45309u : 0xFF0F1B22u;
            secondaryColor = display.isUrgent ? 0xFF92400Eu : 0xFF334155u;
            singleLineColor = primaryColor;
        } else {
            bgColor = display.isUrgent
                ? 0xEB3B260Cu
                : (activeHighlight ? 0x28FFFFFFu : (useOpaqueBackdrop ? 0xFF14181Fu : 0x01000000u));
            primaryColor = display.isUrgent ? pal.gold : 0xFFFFFFFFu;
            secondaryColor = display.isUrgent ? 0xFFFFE59Eu : 0xFFCBD5E1u;
            singleLineColor = primaryColor;
        }
    } else if (settings.useSystemAccentColor || settings.pillColorMode == TaskbarPillColorMode::ThemeAdaptive) {
        if (isLight) {
            if (display.isUrgent) {
                bgColor = 0xF5FEF3C7u;
                borderColor = 0xE0D97706u;
            } else {
                bgColor = activeHighlight ? 0xFFE6EFF6u : (useOpaqueBackdrop ? 0xFFFFFFFFu : 0xF2FFFFFFu);
                borderColor = activeHighlight ? pal.accent : theme::WithAlpha(0xBBu, pal.accent);
            }
            primaryColor = display.isUrgent ? 0xFFB45309u : 0xFF0F172Au;
            secondaryColor = display.isUrgent ? 0xFF92400Eu : 0xFF475569u;
            singleLineColor = primaryColor;
        } else {
            if (display.isUrgent) {
                bgColor = 0xEB3B260Cu;
                borderColor = 0xF2FFCE5Cu;
            } else {
                bgColor = activeHighlight ? 0xF5222A36u : (useOpaqueBackdrop ? 0xFF161B22u : 0xE6161B22u);
                borderColor = activeHighlight ? pal.accent : theme::WithAlpha(0xBBu, pal.accent);
            }
            primaryColor = display.isUrgent ? pal.gold : 0xFFF8FAFCu;
            secondaryColor = display.isUrgent ? 0xFFFFE59Eu : 0xFFCBD5E1u;
            singleLineColor = primaryColor;
        }
    } else {
        // TaskbarPillColorMode::Emerald
        if (isLight) {
            if (display.isUrgent) {
                bgColor = 0xF5FEF3C7u;
                borderColor = 0xE0D97706u;
            } else {
                bgColor = activeHighlight ? 0xFFE0F4F0u : (useOpaqueBackdrop ? 0xFFFFFFFFu : 0xEBFFFFFFu);
                borderColor = activeHighlight ? pal.accent : theme::WithAlpha(0xCCu, pal.accent);
            }
            primaryColor = display.isUrgent ? 0xFFB45309u : 0xFF0B5F58u;
            secondaryColor = display.isUrgent ? 0xFF92400Eu : 0xFF334B57u;
            singleLineColor = display.isUrgent ? 0xFFB45309u : 0xFF0F1B22u;
        } else {
            if (display.isUrgent) {
                bgColor = 0xEB3B260Cu;
                borderColor = 0xF2FFCE5Cu;
            } else {
                bgColor = activeHighlight ? 0xF5154D49u : (useOpaqueBackdrop ? 0xFF0F3634u : 0xDC0E2E2Du);
                borderColor = activeHighlight ? pal.accent : theme::WithAlpha(0xCCu, pal.accent);
            }
            primaryColor = display.isUrgent ? pal.gold : pal.gold;
            secondaryColor = display.isUrgent ? 0xFFFFE59Eu : 0xFFD4E5EEu;
            singleLineColor = display.isUrgent ? pal.gold : 0xFFF4F8FBu;
        }
    }

    float strokeW = (activeHighlight || display.isUrgent) ? std::max(1.2f, 1.35f * s) : std::max(1.0f, 1.2f * s);
    gdip::FillRoundedRect(g, 0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f, radius, bgColor);
    if (drawFrame) {
        gdip::StrokeRoundedRect(g, 0.5f, 0.5f, static_cast<float>(width) - 1.0f, static_cast<float>(height) - 1.0f,
                                radius, borderColor, strokeW);
    }

    float fontMul = GetFontScaleMultiplier(settings.fontSize);
    float bottomReserve = settings.showProgressBar ? 4.5f * s : 0.0f;
    float contentHeight = static_cast<float>(height) - bottomReserve;

    void* family = gdip::CreateBestUiFontFamily(false);
    void* strFormat = nullptr;
    void* brushPrimary = nullptr;
    void* brushSecondary = nullptr;
    void* brushSingle = nullptr;
    void* font1 = nullptr;
    void* font2 = nullptr;

    gdip::GdipCreateStringFormat(0, 0, &strFormat);
    gdip::GdipSetStringFormatFlags(strFormat, gdip::kStringFormatFlagsNoWrap);
    gdip::GdipSetStringFormatTrimming(strFormat, gdip::kStringTrimmingEllipsisCharacter);
    gdip::GdipSetStringFormatLineAlign(strFormat, gdip::kStringAlignmentCenter);
    gdip::GdipCreateSolidFill(primaryColor, &brushPrimary);
    gdip::GdipCreateSolidFill(secondaryColor, &brushSecondary);
    gdip::GdipCreateSolidFill(singleLineColor, &brushSingle);

    if (display.hasLine2) {
        gdip::GdipCreateFont(family, 10.5f * fontMul * s, gdip::kFontStyleBold, gdip::kUnitPixel, &font1);
        gdip::GdipCreateFont(family, 12.5f * fontMul * s, gdip::kFontStyleBold, gdip::kUnitPixel, &font2);

        float textX = 5.0f * s;
        float textW = std::max(12.0f, static_cast<float>(width) - 10.0f * s);
        if (settings.showTaskbarLogo) {
            float iconSz = std::min(22.0f * s, contentHeight - 8.0f * s);
            float iconX = 6.5f * s;
            float iconY = (contentHeight - iconSz) * 0.5f;
            theme::DrawBrandLogoTile(g, iconX, iconY, iconSz);
            gdip::GdipSetStringFormatAlign(strFormat, gdip::kStringAlignmentNear);
            textX = iconX + iconSz + 6.0f * s;
            textW = std::max(12.0f, static_cast<float>(width) - textX - 6.0f * s);
        } else {
            gdip::GdipSetStringFormatAlign(strFormat, gdip::kStringAlignmentCenter);
        }

        float topY = 2.0f * s;
        float halfH = (contentHeight - topY) * 0.5f;
        gdip::RectF rect1{textX, topY, textW, halfH + 1.0f * s};
        gdip::RectF rect2{textX, topY + halfH - 2.0f * s, textW, halfH + 2.0f * s};

        gdip::GdipDrawString(g, display.line1.c_str(), static_cast<int>(display.line1.size()), font1, &rect1, strFormat, brushSecondary);
        gdip::GdipDrawString(g, display.line2.c_str(), static_cast<int>(display.line2.size()), font2, &rect2, strFormat, brushPrimary);
    } else {
        gdip::GdipCreateFont(family, 12.5f * fontMul * s, gdip::kFontStyleBold, gdip::kUnitPixel, &font1);
        float textW = MeasureTextWidth(g, display.line1, font1, strFormat);

        bool showPhase = settings.showTaskbarLogo && display.showPhaseIcon;
        bool showDot = settings.showTaskbarLogo && display.showDot;
        float glyphW = showPhase ? 14.0f * s : (showDot ? 8.0f * s : 0.0f);
        float gap = glyphW > 0.0f ? 5.0f * s : 0.0f;
        float totalContentW = std::min(static_cast<float>(width) - 12.0f * s, glyphW + gap + textW);
        float startX = (static_cast<float>(width) - totalContentW) * 0.5f;
        float centerY = contentHeight * 0.5f;

        if (showPhase) {
            DrawPhaseGlyph(g, startX, centerY - 6.5f * s, 13.0f * s, display.nextPrayer, accentColor);
        } else if (showDot) {
            float dotSize = 7.0f * s;
            void* dotBrush = nullptr;
            gdip::GdipCreateSolidFill(accentColor, &dotBrush);
            if (dotBrush) {
                gdip::GdipFillEllipse(g, dotBrush, startX, centerY - dotSize * 0.5f, dotSize, dotSize);
                gdip::GdipDeleteBrush(dotBrush);
            }
        }

        gdip::GdipSetStringFormatAlign(strFormat, gdip::kStringAlignmentNear);
        float textX = startX + glyphW + gap;
        gdip::RectF rect{textX, 0.0f, std::max(10.0f, static_cast<float>(width) - textX - 5.0f * s), contentHeight};
        gdip::GdipDrawString(g, display.line1.c_str(), static_cast<int>(display.line1.size()), font1, &rect, strFormat, brushSingle);
    }

    if (settings.showProgressBar) {
        float barInset = 7.0f * s;
        float barW = std::max(16.0f * s, static_cast<float>(width) - barInset * 2.0f);
        float barH = std::max(2.2f, 2.5f * s);
        float barY = static_cast<float>(height) - barH - 2.2f * s;

        uint32_t trackCol = isLight ? 0x26000000u : 0x3AFFFFFFu;
        gdip::FillRoundedRect(g, barInset, barY, barW, barH, barH * 0.5f, trackCol);

        float fillW = std::max(barH, barW * static_cast<float>(std::clamp(display.progress, 0.04, 1.0)));
        gdip::FillRoundedRect(g, barInset, barY, fillW, barH, barH * 0.5f, accentColor);
    }

    if (font2) gdip::GdipDeleteFont(font2);
    if (font1) gdip::GdipDeleteFont(font1);
    if (brushSingle) gdip::GdipDeleteBrush(brushSingle);
    if (brushPrimary) gdip::GdipDeleteBrush(brushPrimary);
    if (brushSecondary) gdip::GdipDeleteBrush(brushSecondary);
    if (strFormat) gdip::GdipDeleteStringFormat(strFormat);
    if (family) gdip::GdipDeleteFontFamily(family);
}

uint32_t EnsureVibrantTrayAccent(uint32_t argb, bool isUrgent, bool isLight) {
    if (isUrgent) {
        return isLight ? 0xFFD97706u : 0xFFFBBF24u;
    }
    int r = static_cast<int>((argb >> 16) & 0xFFu);
    int g = static_cast<int>((argb >> 8) & 0xFFu);
    int b = static_cast<int>(argb & 0xFFu);
    int chroma = std::max({r, g, b}) - std::min({r, g, b});
    if (chroma < 42) {
        return isLight ? 0xFF0D9488u : 0xFF2DD4BFu;
    }
    return argb;
}

uint32_t DarkenForWhiteTextContrast(uint32_t argb) {
    int r = static_cast<int>((argb >> 16) & 0xFFu);
    int g = static_cast<int>((argb >> 8) & 0xFFu);
    int b = static_cast<int>(argb & 0xFFu);
    double lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
    if (lum > 0.48) {
        double scale = 0.44 / std::max(0.01, lum);
        r = static_cast<int>(std::clamp(r * scale, 0.0, 255.0));
        g = static_cast<int>(std::clamp(g * scale, 0.0, 255.0));
        b = static_cast<int>(std::clamp(b * scale, 0.0, 255.0));
    }
    return 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

void DrawTrayCornerOverlay(void* g, float fSize, bool isLight, bool audioPlaying, bool isMuted) {
    if (audioPlaying) {
        float pillW = std::max(7.0f, std::round(fSize * 0.44f));
        float pillH = std::max(5.5f, std::round(fSize * 0.31f));
        float px = fSize - pillW - 0.5f;
        float py = 0.5f;
        gdip::FillRoundedRect(g, px, py, pillW, pillH, pillH * 0.45f, 0xFF091319u);
        gdip::StrokeRoundedRect(g, px, py, pillW, pillH, pillH * 0.45f, 0xFFFBBF24u, std::max(0.8f, fSize * 0.035f));

        static constexpr float kBarRatios[3] = {0.50f, 1.0f, 0.72f};
        float innerW = pillW - std::max(2.6f, fSize * 0.14f);
        float barW = std::max(1.1f, innerW / 5.0f);
        float gap = (innerW - barW * 3.0f) * 0.5f;
        float maxH = std::max(2.4f, pillH - std::max(2.2f, fSize * 0.12f));
        float startX = px + (pillW - innerW) * 0.5f;
        for (int i = 0; i < 3; ++i) {
            float bh = std::max(1.5f, maxH * kBarRatios[i]);
            float bx = startX + static_cast<float>(i) * (barW + gap);
            float by = py + (pillH - bh) * 0.5f;
            gdip::FillRoundedRect(g, bx, by, barW, bh, barW * 0.4f, 0xFFFDE047u);
        }
        return;
    }

    if (isMuted) {
        float dotD = std::clamp(std::round(fSize * 0.30f), 5.0f, 11.0f);
        float haloPad = std::max(1.0f, fSize * 0.04f);
        float cx = fSize - dotD * 0.5f - 0.6f;
        float cy = dotD * 0.5f + 0.6f;
        void* haloBr = nullptr;
        if (gdip::GdipCreateSolidFill(isLight ? 0xFFF3F3F3u : 0xFF091319u, &haloBr) == 0 && haloBr) {
            gdip::GdipFillEllipse(g, haloBr, cx - dotD * 0.5f - haloPad, cy - dotD * 0.5f - haloPad,
                                  dotD + haloPad * 2.0f, dotD + haloPad * 2.0f);
            gdip::GdipDeleteBrush(haloBr);
        }
        void* redBr = nullptr;
        if (gdip::GdipCreateSolidFill(0xFFEF4444u, &redBr) == 0 && redBr) {
            gdip::GdipFillEllipse(g, redBr, cx - dotD * 0.5f, cy - dotD * 0.5f, dotD, dotD);
            gdip::GdipDeleteBrush(redBr);
        }
        if (fSize >= 20.0f) {
            float barW = std::max(2.6f, dotD * 0.56f);
            float barH = std::max(1.1f, dotD * 0.18f);
            gdip::FillRoundedRect(g, cx - barW * 0.5f, cy - barH * 0.5f, barW, barH, barH * 0.5f, 0xFFFFFFFFu);
        }
    }
}

void DrawTrayBadge(void* g, int size, const PrayerCountdownState& state, const AppSettings& settings, bool isLight) {
    gdip::GdipGraphicsClear(g, 0x00000000u);
    gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
    gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAliasGridFit);
    gdip::GdipSetPixelOffsetMode(g, gdip::kPixelOffsetModeHighQuality);

    const bool audioPlaying = AudioPlayer::IsPlaying();
    const int nowMin = static_cast<int>(state.calculatedAt.ToEpochSeconds() / 60);
    const bool isMuted = !settings.notificationsEnabled || (settings.notifyMuteUntilMin > nowMin);
    const bool criticalUrgent = state.isUrgent && state.remainingSeconds <= 5 * 60;
    const bool warnUrgent = state.isUrgent && !criticalUrgent;
    const bool kerahatActive = !state.isUrgent && settings.showKerahatIndicator && state.kerahatStatus != KerahatStatus::None;

    uint32_t rawAccent = settings.useSystemAccentColor
        ? GetWindowsAccentColorArgb(!isLight, true)
        : GetPrayerAccentArgb(state.nextPrayer);
    uint32_t textDarkOrLight = isLight ? 0xFF091319u : 0xFFFFFFFFu;
    uint32_t accent = 0;
    uint32_t hdrBg = 0;
    uint32_t digitColor = textDarkOrLight;

    if (criticalUrgent) {
        accent = isLight ? 0xFFDC2626u : 0xFFF87171u;
        hdrBg = 0xFFDC2626u;
        digitColor = isLight ? 0xFFB91C1Cu : 0xFFFCA5A5u;
    } else if (warnUrgent) {
        accent = isLight ? 0xFFD97706u : 0xFFFBBF24u;
        hdrBg = 0xFFD97706u;
        digitColor = isLight ? 0xFFB45309u : 0xFFFDE68Au;
    } else if (kerahatActive) {
        accent = isLight ? 0xFFEA580Cu : 0xFFFB923Cu;
        hdrBg = 0xFFC2410Cu;
    } else {
        accent = EnsureVibrantTrayAccent(rawAccent, false, isLight);
        hdrBg = DarkenForWhiteTextContrast(accent);
    }

    float fSize = static_cast<float>(size);

    if (settings.trayStyle == TrayIconStyle::CrescentBadge || settings.trayStyle == TrayIconStyle::Hidden) {
        float pad = std::max(0.4f, fSize * 0.02f);
        float boxSize = fSize - pad * 2.0f;
        float radius = boxSize * 0.24f;
        constexpr uint32_t kEmeraldTile = 0xFF0F766Eu;
        uint32_t tileBorder = criticalUrgent
            ? 0xFFF87171u
            : (warnUrgent ? 0xFFFBBF24u : (kerahatActive ? 0xFFFB923Cu : 0xFF2DD4BFu));

        gdip::FillRoundedRect(g, pad, pad, boxSize, boxSize, radius, kEmeraldTile);
        gdip::StrokeRoundedRect(g, pad, pad, boxSize, boxSize, radius, tileBorder, std::max(1.0f, fSize * 0.045f));

        // Gerçek dolgun hilal + yıldız geometrisi
        void* moonBrush = nullptr;
        void* cutBrush = nullptr;
        gdip::GdipCreateSolidFill(0xFFFFE07Du, &moonBrush);
        gdip::GdipCreateSolidFill(kEmeraldTile, &cutBrush);
        if (moonBrush) {
            float dOuter = fSize * 0.62f;
            gdip::GdipFillEllipse(g, moonBrush, fSize * 0.47f - dOuter * 0.5f, fSize * 0.52f - dOuter * 0.5f, dOuter, dOuter);
            gdip::GdipDeleteBrush(moonBrush);
        }
        if (cutBrush) {
            float dInner = fSize * 0.50f;
            gdip::GdipFillEllipse(g, cutBrush, fSize * 0.59f - dInner * 0.5f, fSize * 0.44f - dInner * 0.5f, dInner, dInner);
            gdip::GdipDeleteBrush(cutBrush);
        }

        // Hilal boşluğu içinde parlak altın yıldız
        float starX = fSize * 0.68f;
        float starY = fSize * 0.36f;
        void* starBrush = nullptr;
        void* starPen = nullptr;
        gdip::GdipCreateSolidFill(0xFFFFF3B0u, &starBrush);
        gdip::GdipCreatePen1(0xFFFFF3B0u, std::max(1.2f, fSize * 0.048f), gdip::kUnitPixel, &starPen);
        if (starBrush) {
            float sd = fSize * 0.11f;
            gdip::GdipFillEllipse(g, starBrush, starX - sd * 0.5f, starY - sd * 0.5f, sd, sd);
            gdip::GdipDeleteBrush(starBrush);
        }
        if (starPen) {
            float arm = fSize * 0.095f;
            gdip::GdipDrawLine(g, starPen, starX - arm, starY, starX + arm, starY);
            gdip::GdipDrawLine(g, starPen, starX, starY - arm, starX, starY + arm);
            gdip::GdipDeletePen(starPen);
        }
        DrawTrayCornerOverlay(g, fSize, isLight, audioPlaying, isMuted);
        return;
    }

    // Küçük ikonlarda rakam ve Türkçe harflerde maksimum okunabilirlik için Segoe UI Bold
    void* family = nullptr;
    if (gdip::GdipCreateFontFamilyFromName(L"Segoe UI", nullptr, &family) != 0 || !family) {
        family = gdip::CreateBestUiFontFamily(false);
    }

    if (settings.trayStyle == TrayIconStyle::MinutesRing) {
        float pad = std::max(0.4f, fSize * 0.02f);
        float plateDim = fSize - pad * 2.0f;

        void* plateBrush = nullptr;
        gdip::GdipCreateSolidFill(isLight ? 0xFFFFFFFFu : 0xFF091319u, &plateBrush);
        if (plateBrush) {
            gdip::GdipFillEllipse(g, plateBrush, pad, pad, plateDim, plateDim);
            gdip::GdipDeleteBrush(plateBrush);
        }
        if (isLight) {
            void* outerPen = nullptr;
            gdip::GdipCreatePen1(0xFFCBD5E1u, std::max(1.0f, fSize * 0.035f), gdip::kUnitPixel, &outerPen);
            if (outerPen) {
                gdip::GdipDrawEllipse(g, outerPen, pad, pad, plateDim, plateDim);
                gdip::GdipDeletePen(outerPen);
            }
        }

        float ringW = std::clamp(fSize * 0.115f, 1.8f, 4.5f);
        float inset = pad + ringW * 0.5f;
        float ringDim = fSize - inset * 2.0f;

        void* trackPen = nullptr;
        gdip::GdipCreatePen1(isLight ? 0xFFDCE4EAu : 0xFF1C343Fu, ringW, gdip::kUnitPixel, &trackPen);
        if (trackPen) {
            gdip::GdipDrawEllipse(g, trackPen, inset, inset, ringDim, ringDim);
            gdip::GdipDeletePen(trackPen);
        }

        float sweep = std::max(28.0f, static_cast<float>(360.0 * std::clamp(state.progress, 0.08, 1.0)));
        void* arcPen = nullptr;
        gdip::GdipCreatePen1(accent, ringW, gdip::kUnitPixel, &arcPen);
        if (arcPen) {
            gdip::GdipDrawArc(g, arcPen, inset, inset, ringDim, ringDim, -90.0f, sweep);
            gdip::GdipDeletePen(arcPen);
        }

        // Round end-caps on the progress arc
        void* capBrush = nullptr;
        if (gdip::GdipCreateSolidFill(accent, &capBrush) == 0 && capBrush) {
            constexpr float kDegToRad = 3.14159265f / 180.0f;
            float center = fSize * 0.5f;
            float ringR = ringDim * 0.5f;
            float capR = ringW * 0.5f;
            float startRad = -90.0f * kDegToRad;
            float endRad = (-90.0f + sweep) * kDegToRad;
            gdip::GdipFillEllipse(g, capBrush,
                                  center + ringR * std::cos(startRad) - capR,
                                  center + ringR * std::sin(startRad) - capR,
                                  ringW, ringW);
            gdip::GdipFillEllipse(g, capBrush,
                                  center + ringR * std::cos(endRad) - capR,
                                  center + ringR * std::sin(endRad) - capR,
                                  ringW, ringW);
            gdip::GdipDeleteBrush(capBrush);
        }

        auto [topStr, bottomStr] = FormatTrayText(state, settings);
        (void)topStr;
        size_t len = bottomStr.size();
        float fs = fSize * (len >= 3 ? 0.43f : (len == 2 ? 0.55f : 0.63f));
        gdip::DrawTextBoxTight(g, bottomStr, family, fs, true, digitColor, 0.0f, 0.0f, fSize, fSize);
        if (family) gdip::GdipDeleteFontFamily(family);
        DrawTrayCornerOverlay(g, fSize, isLight, audioPlaying, isMuted);
        return;
    }

    if (settings.trayStyle == TrayIconStyle::LargeDigits) {
        float pad = isLight ? 0.4f : 0.0f;
        float boxSize = fSize - pad * 2.0f;
        float radius = std::max(2.0f, fSize * 0.22f);
        uint32_t bodyBg = isLight ? 0xFFFFFFFFu : 0xFF091319u;
        uint32_t borderCol = isLight ? 0xFFCBD5E1u : ((0x66u << 24) | (accent & 0x00FFFFFFu));

        gdip::FillRoundedRect(g, pad, pad, boxSize, boxSize, radius, bodyBg);
        gdip::StrokeRoundedRect(g, pad, pad, boxSize, boxSize, radius, borderCol, 1.0f);

        float barH = std::max(2.0f, std::round(fSize * 0.13f));
        float barInsetX = std::max(1.5f, fSize * 0.12f);
        float barY = fSize - barH - std::max(1.0f, fSize * 0.06f);
        float barW = std::max(4.0f, fSize - barInsetX * 2.0f);
        float barR = barH * 0.5f;

        gdip::FillRoundedRect(g, barInsetX, barY, barW, barH, barR, isLight ? 0xFFE2E8F0u : 0xFF1C343Fu);
        float fillW = std::clamp(static_cast<float>(barW * std::clamp(state.progress, 0.08, 1.0)), barH, barW);
        gdip::FillRoundedRect(g, barInsetX, barY, fillW, barH, barR, accent);

        auto [topStr, bottomStr] = FormatTrayText(state, settings);
        (void)topStr;
        size_t len = bottomStr.size();
        float fs = fSize * (len >= 3 ? 0.50f : (len == 2 ? 0.68f : 0.76f));
        gdip::DrawTextBoxTight(g, bottomStr, family, fs, true, digitColor, 0.0f, 0.0f, fSize, barY);
        if (family) gdip::GdipDeleteFontFamily(family);
        DrawTrayCornerOverlay(g, fSize, isLight, audioPlaying, isMuted);
        return;
    }

    // TwoLineMini: Tam genişlikte, keskin iki parçalı rozet
    float pad = isLight ? 0.4f : 0.0f;
    float boxSize = fSize - pad * 2.0f;
    float radius = std::max(2.0f, fSize * 0.20f);
    uint32_t bodyBg = isLight ? 0xFFFFFFFFu : 0xFF081117u;
    float hdrH = std::round(fSize * 0.39f);

    gdip::FillRoundedRect(g, pad, pad, boxSize, boxSize, radius, bodyBg);
    gdip::FillRoundedRect(g, pad, pad, boxSize, hdrH, radius, hdrBg);
    float seamY = pad + std::min(radius, hdrH * 0.5f);
    gdip::FillRoundedRect(g, pad, seamY, boxSize, hdrH - std::min(radius, hdrH * 0.5f), 0.1f, hdrBg);
    if (isLight) {
        gdip::StrokeRoundedRect(g, pad, pad, boxSize, boxSize, radius, 0xFFCBD5E1u, 1.0f);
    }

    auto [topStr, bottomStr] = FormatTrayText(state, settings);
    gdip::DrawTextBoxTight(g, topStr, family, fSize * (topStr.size() >= 2 ? 0.32f : 0.37f), true, 0xFFFFFFFFu,
                           0.0f, 0.5f, fSize, std::max(4.0f, hdrH - 0.5f));
    size_t bLen = bottomStr.size();
    float bfs = fSize * (bLen >= 3 ? 0.44f : (bLen == 2 ? 0.56f : 0.62f));
    gdip::DrawTextBoxTight(g, bottomStr, family, bfs, true, digitColor,
                           0.0f, hdrH, fSize, std::max(4.0f, fSize - hdrH));

    if (family) gdip::GdipDeleteFontFamily(family);
    DrawTrayCornerOverlay(g, fSize, isLight, audioPlaying, isMuted);
}

} // namespace

int MeasureDesiredPillWidth(const FormattedTaskbarDisplay& display, const AppSettings& settings, double scale) {
    gdip::EnsureInitialized();
    float s = static_cast<float>(scale);
    float fontMul = GetFontScaleMultiplier(settings.fontSize);

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return static_cast<int>(std::llround(96.0 * scale));

    void* g = nullptr;
    void* family = gdip::CreateBestUiFontFamily();
    void* font1 = nullptr;
    void* font2 = nullptr;
    void* strFormat = nullptr;

    gdip::GdipCreateFromHDC(screenDc, &g);
    gdip::GdipCreateStringFormat(0, 0, &strFormat);
    gdip::GdipSetStringFormatFlags(strFormat, gdip::kStringFormatFlagsNoWrap);

    float size1 = display.hasLine2 ? 10.5f * fontMul * s : 12.5f * fontMul * s;
    int style1 = gdip::kFontStyleBold;
    gdip::GdipCreateFont(family, size1, style1, gdip::kUnitPixel, &font1);

    float w1 = MeasureTextWidth(g, display.line1, font1, strFormat);
    float w2 = 0.0f;
    if (display.hasLine2 && !display.line2.empty()) {
        gdip::GdipCreateFont(family, 12.5f * fontMul * s, gdip::kFontStyleBold, gdip::kUnitPixel, &font2);
        w2 = MeasureTextWidth(g, display.line2, font2, strFormat);
    }

    if (font2) gdip::GdipDeleteFont(font2);
    if (font1) gdip::GdipDeleteFont(font1);
    if (strFormat) gdip::GdipDeleteStringFormat(strFormat);
    if (family) gdip::GdipDeleteFontFamily(family);
    if (g) gdip::GdipDeleteGraphics(g);
    ReleaseDC(nullptr, screenDc);

    float textWidth = std::max(w1, w2);
    float iconGap = 0.0f;
    if (settings.showTaskbarLogo) {
        iconGap = display.hasLine2
            ? 26.0f * s
            : (display.showPhaseIcon ? 18.0f * s : (display.showDot ? 13.0f * s : 0.0f));
    }
    float hPad = 18.0f * s;
    int total = static_cast<int>(std::ceil(textWidth + iconGap + hPad));
    int minW = static_cast<int>(std::llround(48.0 * scale));
    int maxW = static_cast<int>(std::llround(245.0 * scale));
    return std::clamp(total, minW, maxW);
}

void RenderPillToLayeredWindow(
    HWND hwnd,
    int x,
    int y,
    int width,
    int height,
    double scale,
    const FormattedTaskbarDisplay& display,
    const AppSettings& settings,
    bool isHovered,
    bool isFlyoutOpen,
    bool isTaskbarLight,
    bool useOpaqueBackdrop) {

    gdip::EnsureInitialized();
    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return;

    HDC memDc = CreateCompatibleDC(screenDc);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (hBitmap && bits) {
        HGDIOBJ oldBitmap = SelectObject(memDc, hBitmap);
        void* gdiBitmap = nullptr;
        void* g = nullptr;
        if (gdip::GdipCreateBitmapFromScan0(width, height, width * 4, gdip::kPixelFormat32bppPARGB, bits, &gdiBitmap) == 0 && gdiBitmap) {
            if (gdip::GdipGetImageGraphicsContext(gdiBitmap, &g) == 0 && g) {
                DrawPillSurface(g, width, height, static_cast<float>(scale), display, settings,
                                isHovered || isFlyoutOpen, isTaskbarLight, useOpaqueBackdrop);
                gdip::GdipDeleteGraphics(g);
            }
            gdip::GdipDisposeImage(gdiBitmap);
        }

        POINT ptDst{x, y};
        SIZE sz{width, height};
        POINT ptSrc{0, 0};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(hwnd, screenDc, &ptDst, &sz, memDc, &ptSrc, 0, &blend, ULW_ALPHA);

        SelectObject(memDc, oldBitmap);
        DeleteObject(hBitmap);
    }

    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

PillRenderResult SmokeTestPillRender(
    const FormattedTaskbarDisplay& display,
    const AppSettings& settings,
    double scale,
    bool useOpaqueBackdrop) {

    gdip::EnsureInitialized();
    int width = MeasureDesiredPillWidth(display, settings, scale);
    int height = static_cast<int>(std::llround(40.0 * scale));

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
    HBITMAP hBitmap = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    PillRenderResult res;

    if (hBitmap && bits) {
        HGDIOBJ oldBitmap = SelectObject(memDc, hBitmap);
        void* gdiBitmap = nullptr;
        void* g = nullptr;
        int st = gdip::GdipCreateBitmapFromScan0(width, height, width * 4, gdip::kPixelFormat32bppPARGB, bits, &gdiBitmap);
        if (st == 0 && gdiBitmap) {
            st = gdip::GdipGetImageGraphicsContext(gdiBitmap, &g);
            if (st == 0 && g) {
                DrawPillSurface(g, width, height, static_cast<float>(scale), display, settings, true, false, useOpaqueBackdrop);
                const auto* px = static_cast<const uint32_t*>(bits);
                int total = width * height;
                int nonZero = 0;
                for (int i = 0; i < total; ++i) {
                    if ((px[i] & 0xFF000000u) != 0) ++nonZero;
                }
                res.status = 0;
                res.nonZeroPixels = nonZero;
                gdip::GdipDeleteGraphics(g);
            } else {
                res.status = st == 0 ? -4 : st;
            }
            gdip::GdipDisposeImage(gdiBitmap);
        } else {
            res.status = st == 0 ? -3 : st;
        }
        SelectObject(memDc, oldBitmap);
        DeleteObject(hBitmap);
    } else {
        res.status = -2;
    }

    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
    return res;
}

HICON CreateTrayIconHandle(
    const PrayerCountdownState& state,
    const AppSettings& settings,
    int iconSize,
    bool isTaskbarLight) {

    gdip::EnsureInitialized();
    int stride = iconSize * 4;
    std::vector<uint8_t> pixels(static_cast<size_t>(stride * iconSize), 0);

    void* gdiBmp = nullptr;
    void* g = nullptr;
    HICON hIcon = nullptr;

    if (gdip::GdipCreateBitmapFromScan0(iconSize, iconSize, stride, gdip::kPixelFormat32bppARGB, pixels.data(), &gdiBmp) == 0 && gdiBmp) {
        if (gdip::GdipGetImageGraphicsContext(gdiBmp, &g) == 0 && g) {
            gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAliasGridFit);
            DrawTrayBadge(g, iconSize, state, settings, isTaskbarLight);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipCreateHICONFromBitmap(gdiBmp, &hIcon);
        gdip::GdipDisposeImage(gdiBmp);
    }
    return hIcon;
}

} // namespace vakit
