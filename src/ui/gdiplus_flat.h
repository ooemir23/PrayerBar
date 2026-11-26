#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstdint>

namespace vakit::gdip {

constexpr int kSmoothingModeAntiAlias = 4;
constexpr int kTextRenderingHintAntiAlias = 3;
constexpr int kTextRenderingHintAntiAliasGridFit = 4;
constexpr int kTextRenderingHintClearTypeGridFit = 5;
constexpr int kCompositingQualityHighQuality = 2;
constexpr int kUnitPixel = 2;
constexpr int kFontStyleRegular = 0;
constexpr int kFontStyleBold = 1;
constexpr int kStringAlignmentNear = 0;
constexpr int kStringAlignmentCenter = 1;
constexpr int kStringAlignmentFar = 2;
constexpr int kStringFormatFlagsNoWrap = 0x00001000;
constexpr int kStringTrimmingNone = 0;
constexpr int kStringTrimmingEllipsisCharacter = 3;
constexpr int kPixelOffsetModeHighQuality = 2;
constexpr int kFillModeAlternate = 0;
constexpr int kPixelFormat32bppPARGB = 0x000E200B;
constexpr int kPixelFormat32bppARGB = 0x0026200A;

struct RectF {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct GdiplusStartupInput {
    uint32_t GdiplusVersion = 1;
    void* DebugEventCallback = nullptr;
    BOOL SuppressBackgroundThread = FALSE;
    BOOL SuppressExternalCodecs = FALSE;
};

extern "C" {
int __stdcall GdiplusStartup(ULONG_PTR* token, const GdiplusStartupInput* input, void* output);
void __stdcall GdiplusShutdown(ULONG_PTR token);
int __stdcall GdipCreateFromHDC(HDC hdc, void** graphics);
int __stdcall GdipCreateBitmapFromScan0(int width, int height, int stride, int format, void* scan0, void** bitmap);
int __stdcall GdipGetImageGraphicsContext(void* image, void** graphics);
int __stdcall GdipCreateHICONFromBitmap(void* bitmap, HICON* hicon);
int __stdcall GdipDisposeImage(void* image);
int __stdcall GdipDeleteGraphics(void* graphics);
int __stdcall GdipSetSmoothingMode(void* graphics, int smoothingMode);
int __stdcall GdipSetTextRenderingHint(void* graphics, int mode);
int __stdcall GdipSetCompositingQuality(void* graphics, int quality);
int __stdcall GdipSetPixelOffsetMode(void* graphics, int pixelOffsetMode);
int __stdcall GdipGraphicsClear(void* graphics, uint32_t color);
int __stdcall GdipCreateSolidFill(uint32_t color, void** brush);
int __stdcall GdipDeleteBrush(void* brush);
int __stdcall GdipCreatePen1(uint32_t color, float width, int unit, void** pen);
int __stdcall GdipDeletePen(void* pen);
int __stdcall GdipCreateFontFamilyFromName(const wchar_t* name, void* fontCollection, void** fontFamily);
int __stdcall GdipDeleteFontFamily(void* fontFamily);
int __stdcall GdipCreateFont(void* fontFamily, float emSize, int style, int unit, void** font);
int __stdcall GdipDeleteFont(void* font);
int __stdcall GdipCreateStringFormat(int formatAttributes, int language, void** format);
int __stdcall GdipStringFormatGetGenericTypographic(void** format);
int __stdcall GdipSetStringFormatAlign(void* format, int align);
int __stdcall GdipSetStringFormatLineAlign(void* format, int align);
int __stdcall GdipSetStringFormatFlags(void* format, int flags);
int __stdcall GdipSetStringFormatTrimming(void* format, int trimming);
int __stdcall GdipDeleteStringFormat(void* format);
int __stdcall GdipDrawString(void* graphics, const wchar_t* text, int length, void* font, const RectF* layoutRect, void* stringFormat, void* brush);
int __stdcall GdipMeasureString(void* graphics, const wchar_t* text, int length, void* font, const RectF* layoutRect, void* stringFormat, RectF* boundingBox, int* codepointsFitted, int* linesFilled);
int __stdcall GdipCreatePath(int fillMode, void** path);
int __stdcall GdipAddPathRectangle(void* path, float x, float y, float width, float height);
int __stdcall GdipAddPathArc(void* path, float x, float y, float width, float height, float startAngle, float sweepAngle);
int __stdcall GdipClosePathFigure(void* path);
int __stdcall GdipFillPath(void* graphics, void* brush, void* path);
int __stdcall GdipDrawPath(void* graphics, void* pen, void* path);
int __stdcall GdipDeletePath(void* path);
int __stdcall GdipFillEllipse(void* graphics, void* brush, float x, float y, float width, float height);
int __stdcall GdipDrawEllipse(void* graphics, void* pen, float x, float y, float width, float height);
int __stdcall GdipDrawArc(void* graphics, void* pen, float x, float y, float width, float height, float startAngle, float sweepAngle);
int __stdcall GdipDrawLine(void* graphics, void* pen, float x1, float y1, float x2, float y2);
}

inline ULONG_PTR& TokenStorage() {
    static ULONG_PTR token = 0;
    return token;
}

inline void EnsureInitialized() {
    if (TokenStorage() != 0) return;
    GdiplusStartupInput input;
    GdiplusStartup(&TokenStorage(), &input, nullptr);
}

inline void Shutdown() {
    if (TokenStorage() != 0) {
        GdiplusShutdown(TokenStorage());
        TokenStorage() = 0;
    }
}

inline void* CreateRoundedRectPath(float x, float y, float w, float h, float radius) {
    void* path = nullptr;
    GdipCreatePath(kFillModeAlternate, &path);
    if (!path) return nullptr;
    float d = std::min(radius * 2.0f, std::min(w, h));
    if (d <= 0.5f) {
        GdipAddPathRectangle(path, x, y, w, h);
        return path;
    }
    GdipAddPathArc(path, x, y, d, d, 180.0f, 90.0f);
    GdipAddPathArc(path, x + w - d, y, d, d, 270.0f, 90.0f);
    GdipAddPathArc(path, x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    GdipAddPathArc(path, x, y + h - d, d, d, 90.0f, 90.0f);
    GdipClosePathFigure(path);
    return path;
}

inline void* CreateBestUiFontFamily(bool display = false) {
    (void)display;
    void* family = nullptr;
    // Static Segoe UI scales cleanly in GDI+ without variable-font axis artifacts.
    if (GdipCreateFontFamilyFromName(L"Segoe UI", nullptr, &family) == 0 && family) return family;
    if (GdipCreateFontFamilyFromName(L"Segoe UI Variable Text", nullptr, &family) == 0 && family) return family;
    if (GdipCreateFontFamilyFromName(L"Segoe UI Variable Display", nullptr, &family) == 0 && family) return family;
    GdipCreateFontFamilyFromName(L"Arial", nullptr, &family);
    return family;
}

inline void* CreateFluentIconFontFamily() {
    void* family = nullptr;
    if (GdipCreateFontFamilyFromName(L"Segoe Fluent Icons", nullptr, &family) == 0 && family) return family;
    if (GdipCreateFontFamilyFromName(L"Segoe MDL2 Assets", nullptr, &family) == 0 && family) return family;
    return nullptr;
}

inline void FillRoundedRect(void* g, float x, float y, float w, float h, float r, uint32_t argb) {
    void* path = CreateRoundedRectPath(x, y, w, h, r);
    if (!path) return;
    void* brush = nullptr;
    if (GdipCreateSolidFill(argb, &brush) == 0 && brush) {
        GdipFillPath(g, brush, path);
        GdipDeleteBrush(brush);
    }
    GdipDeletePath(path);
}

inline void StrokeRoundedRect(void* g, float x, float y, float w, float h, float r, uint32_t argb, float strokeW) {
    void* path = CreateRoundedRectPath(x, y, w, h, r);
    if (!path) return;
    void* pen = nullptr;
    if (GdipCreatePen1(argb, strokeW, kUnitPixel, &pen) == 0 && pen) {
        GdipDrawPath(g, pen, path);
        GdipDeletePen(pen);
    }
    GdipDeletePath(path);
}

inline void DrawTextBox(
    void* g,
    const std::wstring& text,
    void* family,
    float pxSize,
    bool bold,
    uint32_t argb,
    float x,
    float y,
    float w,
    float h,
    int hAlign,
    int vAlign) {

    if (!g || !family || text.empty()) return;
    void* font = nullptr;
    void* fmt = nullptr;
    void* brush = nullptr;
    GdipCreateFont(family, std::max(6.0f, pxSize), bold ? kFontStyleBold : kFontStyleRegular, kUnitPixel, &font);
    GdipCreateStringFormat(0, 0, &fmt);
    GdipSetStringFormatAlign(fmt, hAlign);
    GdipSetStringFormatLineAlign(fmt, vAlign);
    GdipSetStringFormatFlags(fmt, kStringFormatFlagsNoWrap);
    GdipSetStringFormatTrimming(fmt, kStringTrimmingEllipsisCharacter);
    GdipCreateSolidFill(argb, &brush);
    if (font && fmt && brush) {
        RectF rc{x, y, w, h};
        GdipDrawString(g, text.c_str(), static_cast<int>(text.size()), font, &rc, fmt, brush);
    }
    if (brush) GdipDeleteBrush(brush);
    if (fmt) GdipDeleteStringFormat(fmt);
    if (font) GdipDeleteFont(font);
}

inline void DrawFluentIcon(
    void* g,
    void* iconFamily,
    const wchar_t* fluentGlyph,
    void* uiFamily,
    const wchar_t* fallbackText,
    float pxSize,
    uint32_t argb,
    float x,
    float y,
    float w,
    float h) {

    if (!g) return;
    if (iconFamily && fluentGlyph && fluentGlyph[0]) {
        DrawTextBox(g, fluentGlyph, iconFamily, pxSize, false, argb, x, y, w, h,
                    kStringAlignmentCenter, kStringAlignmentCenter);
    } else if (uiFamily && fallbackText && fallbackText[0]) {
        DrawTextBox(g, fallbackText, uiFamily, pxSize, false, argb, x, y, w, h,
                    kStringAlignmentCenter, kStringAlignmentCenter);
    }
}

inline void DrawTextBoxTight(
    void* g,
    const std::wstring& text,
    void* family,
    float pxSize,
    bool bold,
    uint32_t argb,
    float x,
    float y,
    float w,
    float h) {

    if (!g || !family || text.empty()) return;
    void* font = nullptr;
    void* fmt = nullptr;
    void* brush = nullptr;
    GdipCreateFont(family, std::max(6.0f, pxSize), bold ? kFontStyleBold : kFontStyleRegular, kUnitPixel, &font);
    if (GdipStringFormatGetGenericTypographic(&fmt) != 0 || !fmt) {
        GdipCreateStringFormat(0, 0, &fmt);
    }
    if (fmt) {
        GdipSetStringFormatAlign(fmt, kStringAlignmentCenter);
        GdipSetStringFormatLineAlign(fmt, kStringAlignmentCenter);
        GdipSetStringFormatFlags(fmt, kStringFormatFlagsNoWrap);
        GdipSetStringFormatTrimming(fmt, kStringTrimmingNone);
    }
    GdipCreateSolidFill(argb, &brush);
    if (font && fmt && brush) {
        RectF rc{x, y, w, h};
        GdipDrawString(g, text.c_str(), static_cast<int>(text.size()), font, &rc, fmt, brush);
    }
    if (brush) GdipDeleteBrush(brush);
    if (fmt) GdipDeleteStringFormat(fmt);
    if (font) GdipDeleteFont(font);
}

} // namespace vakit::gdip
