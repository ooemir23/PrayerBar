#pragma once

#include "gdiplus_flat.h"
#include "../platform/theme_utils.h"

#include <algorithm>
#include <cstdint>

namespace vakit::theme {

inline uint32_t WithAlpha(uint8_t a, uint32_t argb) {
    return (static_cast<uint32_t>(a) << 24) | (argb & 0x00FFFFFFu);
}

inline COLORREF ArgbToColorRef(uint32_t argb) {
    return RGB((argb >> 16) & 0xFFu, (argb >> 8) & 0xFFu, argb & 0xFFu);
}

struct UiPalette {
    bool isDark = true;
    uint32_t bgWindow = 0;
    uint32_t bgSidebar = 0;
    uint32_t bgSurface = 0;
    uint32_t bgCard = 0;
    uint32_t bgCardAlt = 0;
    uint32_t bgHighlight = 0;
    uint32_t bgInput = 0;
    uint32_t bgBtn = 0;
    uint32_t bgBtnHover = 0;
    uint32_t bgBtnPressed = 0;
    uint32_t bgNavActive = 0;
    uint32_t bgNavHover = 0;

    uint32_t border = 0;
    uint32_t borderSubtle = 0;
    uint32_t borderAccent = 0;
    uint32_t borderAccentSoft = 0;

    uint32_t accent = 0;
    uint32_t accentTextOnFill = 0;
    uint32_t gold = 0;
    uint32_t goldSoftBg = 0;

    uint32_t textPrimary = 0;
    uint32_t textSecondary = 0;
    uint32_t textMuted = 0;

    uint32_t toggleOffTrack = 0;
    uint32_t toggleOffKnob = 0;
    uint32_t toggleOnKnob = 0;

    uint32_t success = 0;
    uint32_t danger = 0;
    uint32_t amberBg = 0;
    uint32_t amberBorder = 0;
    uint32_t chipBg = 0;
};

inline UiPalette GetUiPalette(bool isDark, bool useSystemAccent) {
    UiPalette p;
    p.isDark = isDark;
    p.accent = GetWindowsAccentColorArgb(isDark, useSystemAccent);

    if (isDark) {
        p.bgWindow = 0xFF10171Eu;
        p.bgSidebar = 0xFF0C1218u;
        p.bgSurface = 0xFF161F28u;
        p.bgCard = 0xFF1C2733u;
        p.bgCardAlt = 0xFF18222Cu;
        p.bgHighlight = 0xFF13403Du;
        p.bgInput = 0xFF131C25u;
        p.bgBtn = 0xFF233240u;
        p.bgBtnHover = 0xFF2C3F50u;
        p.bgBtnPressed = 0xFF1B2834u;
        p.bgNavActive = 0xFF143C39u;
        p.bgNavHover = 0xFF18242Fu;

        p.border = 0xFF2E4252u;
        p.borderSubtle = 0xFF243442u;
        p.borderAccent = 0xFF2DD4BFu;
        p.borderAccentSoft = 0xAA2DD4BFu;

        p.accentTextOnFill = 0xFF071615u;
        p.gold = 0xFFFFCE5Cu;
        p.goldSoftBg = 0x28FFCE5Cu;

        p.textPrimary = 0xFFF4F8FBu;
        p.textSecondary = 0xFFB0C2D2u;
        p.textMuted = 0xFF8CA1B5u;

        p.toggleOffTrack = 0xFF2A3B4Au;
        p.toggleOffKnob = 0xFFCBD8E4u;
        p.toggleOnKnob = 0xFF0A141Bu;

        p.success = 0xFF34D399u;
        p.danger = 0xFFF87171u;
        p.amberBg = 0x2CF59E0Bu;
        p.amberBorder = 0x99FBBF24u;
        p.chipBg = 0xFF22313Fu;
    } else {
        p.bgWindow = 0xFFF2F6F7u;
        p.bgSidebar = 0xFFE7EFF0u;
        p.bgSurface = 0xFFF7FAFAu;
        p.bgCard = 0xFFFFFFFFu;
        p.bgCardAlt = 0xFFF4F8F9u;
        p.bgHighlight = 0xFFDCF4F0u;
        p.bgInput = 0xFFFFFFFFu;
        p.bgBtn = 0xFFF0F5F6u;
        p.bgBtnHover = 0xFFE1ECEEu;
        p.bgBtnPressed = 0xFFD4E4E7u;
        p.bgNavActive = 0xFFD5F1ECu;
        p.bgNavHover = 0xFFDFEAEBu;

        p.border = 0xFFC5D6DAu;
        p.borderSubtle = 0xFFDCE6E9u;
        p.borderAccent = 0xFF0D9488u;
        p.borderAccentSoft = 0x880D9488u;

        p.accentTextOnFill = 0xFFFFFFFFu;
        p.gold = 0xFFB45309u;
        p.goldSoftBg = 0x22D97706u;

        p.textPrimary = 0xFF0F1B22u;
        p.textSecondary = 0xFF425666u;
        p.textMuted = 0xFF607484u;

        p.toggleOffTrack = 0xFFB8C9CEu;
        p.toggleOffKnob = 0xFFFFFFFFu;
        p.toggleOnKnob = 0xFFFFFFFFu;

        p.success = 0xFF059669u;
        p.danger = 0xFFDC2626u;
        p.amberBg = 0x22D97706u;
        p.amberBorder = 0x88D97706u;
        p.chipBg = 0xFFE6EFF1u;
    }

    if (useSystemAccent) {
        p.borderAccent = p.accent;
        p.borderAccentSoft = WithAlpha(0xAAu, p.accent);
        if (isDark) {
            p.bgHighlight = WithAlpha(0x36u, p.accent);
            p.bgNavActive = WithAlpha(0x30u, p.accent);
        } else {
            p.bgHighlight = WithAlpha(0x22u, p.accent);
            p.bgNavActive = WithAlpha(0x24u, p.accent);
        }
    }

    return p;
}

inline void DrawBrandLogoTile(void* g, float x, float y, float size) {
    if (!g || size <= 2.0f) return;

    constexpr uint32_t kTileFill = 0xFF0F5E58u;
    constexpr uint32_t kTileBorder = 0x702DD4BFu;
    constexpr uint32_t kRingTrack = 0x50FFFFFFu;
    constexpr uint32_t kGoldArc = 0xFFFFCE5Cu;
    constexpr uint32_t kTipDot = 0xFFFFF6D1u;

    float radius = size * 0.24f;
    gdip::FillRoundedRect(g, x, y, size, size, radius, kTileFill);
    gdip::StrokeRoundedRect(g, x, y, size, size, radius, kTileBorder, std::max(1.0f, size * 0.045f));

    float cx = x + size * 0.50f;
    float cy = y + size * 0.50f;
    float ringR = size * 0.32f;
    float ringW = std::max(1.4f, size * 0.075f);

    void* trackPen = nullptr;
    if (gdip::GdipCreatePen1(kRingTrack, ringW, gdip::kUnitPixel, &trackPen) == 0 && trackPen) {
        gdip::GdipDrawEllipse(g, trackPen, cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f);
        gdip::GdipDeletePen(trackPen);
    }

    void* arcPen = nullptr;
    if (gdip::GdipCreatePen1(kGoldArc, ringW, gdip::kUnitPixel, &arcPen) == 0 && arcPen) {
        gdip::GdipDrawArc(g, arcPen, cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f, -90.0f, 270.0f);
        gdip::GdipDeletePen(arcPen);
    }

    void* tipBrush = nullptr;
    if (gdip::GdipCreateSolidFill(kTipDot, &tipBrush) == 0 && tipBrush) {
        float tipR = ringW * 0.85f;
        gdip::GdipFillEllipse(g, tipBrush, cx - ringR - tipR, cy - tipR, tipR * 2.0f, tipR * 2.0f);
        gdip::GdipDeleteBrush(tipBrush);
    }

    // Crisp golden crescent inside the ring
    float moonR = size * 0.18f;
    float outerX = cx - moonR;
    float outerY = cy - moonR;
    void* moonBrush = nullptr;
    void* cutBrush = nullptr;
    if (gdip::GdipCreateSolidFill(kGoldArc, &moonBrush) == 0 && moonBrush) {
        gdip::GdipFillEllipse(g, moonBrush, outerX, outerY, moonR * 2.0f, moonR * 2.0f);
        gdip::GdipDeleteBrush(moonBrush);
    }
    if (gdip::GdipCreateSolidFill(kTileFill, &cutBrush) == 0 && cutBrush) {
        float innerR = moonR * 0.80f;
        float innerX = outerX + moonR * 0.52f;
        float innerY = outerY - moonR * 0.14f;
        gdip::GdipFillEllipse(g, cutBrush, innerX, innerY, innerR * 2.0f, innerR * 2.0f);
        gdip::GdipDeleteBrush(cutBrush);
    }
}

inline void DrawFluentToggleSwitch(void* g, float x, float y, float w, float h, bool checked, const UiPalette& pal) {
    if (!g || w <= 4.0f || h <= 4.0f) return;
    float radius = h * 0.5f;

    if (checked) {
        gdip::FillRoundedRect(g, x, y, w, h, radius, pal.accent);
    } else {
        gdip::FillRoundedRect(g, x, y, w, h, radius, pal.toggleOffTrack);
        gdip::StrokeRoundedRect(g, x, y, w, h, radius, pal.border, std::max(1.0f, h * 0.06f));
    }

    float pad = std::max(2.0f, h * 0.15f);
    float knobD = h - pad * 2.0f;
    float knobX = checked ? (x + w - pad - knobD) : (x + pad);
    float knobY = y + pad;
    void* knobBrush = nullptr;
    if (gdip::GdipCreateSolidFill(checked ? pal.toggleOnKnob : pal.toggleOffKnob, &knobBrush) == 0 && knobBrush) {
        gdip::GdipFillEllipse(g, knobBrush, knobX, knobY, knobD, knobD);
        gdip::GdipDeleteBrush(knobBrush);
    }
}

} // namespace vakit::theme
