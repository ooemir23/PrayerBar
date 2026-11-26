#include "context_menu.h"
#include "gdiplus_flat.h"
#include "theme_palette.h"
#include "../core/formatter.h"
#include "../core/i18n.h"
#include "../core/schedule_service.h"
#include "../platform/audio_player.h"
#include "../platform/theme_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace vakit {

namespace {

constexpr wchar_t kContextMenuClassName[] = L"PrayerBarContextMenu";
constexpr float kMenuWidthDip = 296.0f;

std::wstring FormatSingleArg(const wchar_t* pattern, const std::wstring& arg) {
    wchar_t buf[512]{};
    swprintf_s(buf, pattern, arg.c_str());
    return std::wstring(buf);
}

} // namespace

ContextMenuPopup::ContextMenuPopup() {
    static bool registered = false;
    if (!registered) {
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = WndProcStatic;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kContextMenuClassName;
        RegisterClassExW(&wc);
        registered = true;
    }
}

ContextMenuPopup::~ContextMenuPopup() {
    CloseNow();
}

void ContextMenuPopup::Show(
    int anchorScreenX,
    int anchorScreenY,
    const RECT& bandRect,
    bool bandVisible,
    const AppSettings& settings,
    const PrayerCountdownState* latestState,
    bool isPaidPro,
    ContextMenuCallbacks callbacks) {

    if (hwnd_) {
        CloseNow();
        return;
    }

    closing_ = false;
    trackingLeave_ = false;
    notifExpanded_ = false;
    hoverIndex_ = -1;
    focusIndex_ = -1;

    anchorScreenX_ = anchorScreenX;
    anchorScreenY_ = anchorScreenY;
    bandRect_ = bandRect;
    bandVisible_ = bandVisible;
    settings_ = settings;
    isPaidPro_ = isPaidPro;
    callbacks_ = std::move(callbacks);
    hasState_ = (latestState != nullptr);
    if (latestState) {
        state_ = *latestState;
    }

    HMONITOR mon = (bandVisible_ && !IsRectEmpty(&bandRect_))
        ? MonitorFromRect(&bandRect_, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint({anchorScreenX_, anchorScreenY_}, MONITOR_DEFAULTTONEAREST);
    UINT dpi = GetDpiForMonitorCompat(mon);
    scale_ = dpi >= 96 ? static_cast<double>(dpi) / 96.0 : 1.0;

    int winW = 0;
    int winH = 0;
    RebuildLayout(winW, winH);

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kContextMenuClassName,
        L"PrayerBarMenu",
        WS_POPUP,
        0, 0, winW, winH,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return;

    bool isLight = IsAppLight(settings_.themeMode);
    ApplyWindowDwmStyle(hwnd_, !isLight, 2 /*DWMWCP_ROUND*/, 3 /*DWMSBT_TRANSIENTWINDOW*/);

    PositionWindow(winW, winH);
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ContextMenuPopup::CloseNow() {
    if (!hwnd_) return;
    if (closing_) return;
    closing_ = true;
    HWND h = hwnd_;
    hwnd_ = nullptr;
    DestroyWindow(h);
    lastClosedTick_ = GetTickCount64();
    if (settings_.autoOptimizeMemory) {
        TrimWorkingSet();
    }
}

void ContextMenuPopup::EnsureOnTop() {
    if (!hwnd_ || !IsWindowVisible(hwnd_)) return;
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void ContextMenuPopup::RebuildLayout(int& outWidth, int& outHeight) {
    items_.clear();
    notifSubCardRc_ = {};

    LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
    int nowMin = static_cast<int>(now.ToEpochSeconds() / 60);
    mutedUntilMin_ = settings_.notifyMuteUntilMin;
    isMuted_ = mutedUntilMin_ > nowMin;

    const float s = static_cast<float>(scale_);
    const float pad = 8.0f * s;
    const float menuW = kMenuWidthDip * s;
    const float contentW = menuW - pad * 2.0f;

    auto S = [](float v) -> LONG { return static_cast<LONG>(std::llround(v)); };
    auto pushItem = [&](MenuAction action, float x, float y, float w, float h, bool inSub = false) {
        items_.push_back({action, {S(x), S(y), S(x + w), S(y + h)}, inSub});
    };

    float y = pad;

    // 1) Header card
    const float headerH = 54.0f * s;
    pushItem(MenuAction::HeaderFlyout, pad, y, contentW, headerH);
    y += headerH + 6.0f * s;

    // 2) Stop audio pill when playing
    if (AudioPlayer::IsPlaying()) {
        const float stopH = 34.0f * s;
        pushItem(MenuAction::StopSound, pad, y, contentW, stopH);
        y += stopH + 6.0f * s;
    }

    // 3) Core daily actions
    const float rowH = 33.0f * s;
    const float rowGap = 2.0f * s;
    pushItem(MenuAction::Imsakiye, pad, y, contentW, rowH);
    y += rowH + rowGap;
    pushItem(MenuAction::Location, pad, y, contentW, rowH);
    y += rowH + rowGap;
    pushItem(MenuAction::Refresh, pad, y, contentW, rowH);
    y += rowH + 5.0f * s;

    // Divider 1
    divider1Y_ = y;
    y += 6.0f * s;

    // 4) Notifications accordion header
    pushItem(MenuAction::NotifToggleAccordion, pad, y, contentW, rowH);
    y += rowH;

    if (notifExpanded_) {
        y += 4.0f * s;
        const float subPad = 5.0f * s;
        const float subX = pad + 2.0f * s;
        const float subW = contentW - 4.0f * s;
        const float subRowX = subX + subPad;
        const float subRowW = subW - subPad * 2.0f;
        const float subRowH = 30.0f * s;
        const float subTop = y;

        y += subPad;
        pushItem(MenuAction::NotifAllSwitch, subRowX, y, subRowW, subRowH, true);
        y += subRowH + 2.0f * s;

        if (isMuted_) {
            y += 18.0f * s;  // muted-until caption
            pushItem(MenuAction::NotifUnmute, subRowX, y, subRowW, subRowH, true);
            y += subRowH + 2.0f * s;
        }
        pushItem(MenuAction::NotifMute1h, subRowX, y, subRowW, subRowH, true);
        y += subRowH + 2.0f * s;
        pushItem(MenuAction::NotifMuteToday, subRowX, y, subRowW, subRowH, true);
        y += subRowH + 2.0f * s;
        pushItem(MenuAction::NotifTest, subRowX, y, subRowW, subRowH, true);
        y += subRowH + subPad;

        notifSubCardRc_ = {S(subX), S(subTop), S(subX + subW), S(y)};
        y += 4.0f * s;
    } else {
        y += rowGap;
    }

    // 5) Settings + optional Pro
    pushItem(MenuAction::Settings, pad, y, contentW, rowH);
    y += rowH;

    if (!isPaidPro_) {
        y += rowGap;
        pushItem(MenuAction::Pro, pad, y, contentW, rowH);
        y += rowH;
    }

    y += 5.0f * s;
    divider2Y_ = y;
    y += 6.0f * s;

    // 6) Exit
    pushItem(MenuAction::Exit, pad, y, contentW, rowH);
    y += rowH + pad;

    outWidth = static_cast<int>(std::llround(menuW));
    outHeight = static_cast<int>(std::llround(y));
}

void ContextMenuPopup::PositionWindow(int winW, int winH) {
    if (!hwnd_) return;

    HMONITOR mon = (bandVisible_ && !IsRectEmpty(&bandRect_))
        ? MonitorFromRect(&bandRect_, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint({anchorScreenX_, anchorScreenY_}, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
        mi.rcMonitor = mi.rcWork;
    }

    HWND taskbarHwnd = FindWindowW(L"Shell_TrayWnd", nullptr);
    RECT tbRect{};
    bool hasTb = taskbarHwnd && GetWindowRect(taskbarHwnd, &tbRect);

    const int monW = static_cast<int>(mi.rcMonitor.right - mi.rcMonitor.left);
    const int monH = static_cast<int>(mi.rcMonitor.bottom - mi.rcMonitor.top);
    const int midY = static_cast<int>(mi.rcMonitor.top) + monH / 2;
    const int midX = static_cast<int>(mi.rcMonitor.left) + monW / 2;

    const bool horizontalTb = !hasTb || ((tbRect.right - tbRect.left) >= (tbRect.bottom - tbRect.top));
    const bool topTb = hasTb && horizontalTb && ((tbRect.top + tbRect.bottom) / 2 < midY);
    const bool leftTb = hasTb && !horizontalTb && ((tbRect.left + tbRect.right) / 2 < midX);
    const bool rightTb = hasTb && !horizontalTb && !leftTb;

    const int gap = static_cast<int>(std::llround(10.0 * scale_));
    const int padX = static_cast<int>(std::llround(8.0 * scale_));

    int centerX = anchorScreenX_;
    if (bandVisible_ && !IsRectEmpty(&bandRect_) &&
        anchorScreenX_ >= bandRect_.left - 24 && anchorScreenX_ <= bandRect_.right + 24 &&
        anchorScreenY_ >= bandRect_.top - 24 && anchorScreenY_ <= bandRect_.bottom + 24) {
        centerX = static_cast<int>(bandRect_.left + bandRect_.right) / 2;
    }

    int x = centerX - winW / 2;
    if (leftTb && bandVisible_ && !IsRectEmpty(&bandRect_) && bandRect_.right < static_cast<int>(mi.rcMonitor.left) + monW / 3) {
        x = std::max<int>(x, static_cast<int>(bandRect_.right) + gap);
    } else if (rightTb && bandVisible_ && !IsRectEmpty(&bandRect_) && bandRect_.left > static_cast<int>(mi.rcMonitor.right) - monW / 3) {
        x = std::min<int>(x, static_cast<int>(bandRect_.left) - gap - winW);
    }

    const int minX = static_cast<int>(mi.rcWork.left) + padX;
    const int maxX = std::max<int>(minX, static_cast<int>(mi.rcWork.right) - winW - padX);
    x = std::clamp(x, minX, maxX);

    int y = 0;
    if (topTb) {
        int topLimit = static_cast<int>(mi.rcWork.top) + gap;
        if (bandVisible_ && !IsRectEmpty(&bandRect_) && bandRect_.bottom < midY) {
            topLimit = std::max<int>(topLimit, static_cast<int>(bandRect_.bottom) + gap);
        }
        const int maxY = std::max<int>(topLimit, static_cast<int>(mi.rcWork.bottom) - winH - gap);
        y = std::min<int>(topLimit, maxY);
    } else {
        int bottomLimit = static_cast<int>(mi.rcWork.bottom) - gap;
        if (bandVisible_ && !IsRectEmpty(&bandRect_) && bandRect_.top > midY) {
            bottomLimit = std::min<int>(bottomLimit, static_cast<int>(bandRect_.top) - gap);
        }
        const int minY = static_cast<int>(mi.rcWork.top) + gap;
        y = std::max<int>(minY, bottomLimit - winH);
    }

    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, winW, winH, SWP_NOACTIVATE);
}

int ContextMenuPopup::HitTestIndex(int x, int y) const {
    POINT pt{x, y};
    for (size_t i = 0; i < items_.size(); ++i) {
        if (PtInRect(&items_[i].rc, pt)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ContextMenuPopup::TriggerAction(MenuAction action) {
    if (action == MenuAction::None) return;

    if (action == MenuAction::NotifToggleAccordion) {
        notifExpanded_ = !notifExpanded_;
        int winW = 0;
        int winH = 0;
        RebuildLayout(winW, winH);
        PositionWindow(winW, winH);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (action == MenuAction::NotifAllSwitch) {
        bool nextEnabled = !settings_.notificationsEnabled;
        settings_.SetAllNotificationsEnabled(nextEnabled);
        auto cb = callbacks_.onToggleAllNotifications;
        if (cb) cb(nextEnabled);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    std::function<void()> fn;
    switch (action) {
        case MenuAction::HeaderFlyout:   fn = callbacks_.onToggleFlyout; break;
        case MenuAction::StopSound:      fn = callbacks_.onStopSound; break;
        case MenuAction::Imsakiye:       fn = callbacks_.onOpenImsakiye; break;
        case MenuAction::Location:       fn = callbacks_.onChangeLocation; break;
        case MenuAction::Refresh:        fn = callbacks_.onRefresh; break;
        case MenuAction::NotifUnmute:    fn = callbacks_.onUnmute; break;
        case MenuAction::NotifMute1h:    fn = callbacks_.onMute1h; break;
        case MenuAction::NotifMuteToday: fn = callbacks_.onMuteToday; break;
        case MenuAction::NotifTest:      fn = callbacks_.onTestNotification; break;
        case MenuAction::Settings:       fn = callbacks_.onOpenSettings; break;
        case MenuAction::Pro:            fn = callbacks_.onOpenPro; break;
        case MenuAction::Exit:           fn = callbacks_.onExit; break;
        default: break;
    }

    CloseNow();
    if (fn) fn();
}

void ContextMenuPopup::PaintClient(HDC hdc, int width, int height) {
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
            gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAlias);
            gdip::GdipSetCompositingQuality(g, gdip::kCompositingQualityHighQuality);

            const bool isLight = IsAppLight(settings_.themeMode);
            const auto pal = theme::GetUiPalette(!isLight, settings_.useSystemAccentColor);
            const AppLanguage lang = settings_.language;
            const bool rtl = IsRtlLanguage(lang);
            const float s = static_cast<float>(scale_);
            const float w = static_cast<float>(width);
            const float h = static_cast<float>(height);

            gdip::GdipGraphicsClear(g, pal.bgWindow);
            gdip::StrokeRoundedRect(g, 0.6f, 0.6f, w - 1.2f, h - 1.2f, 10.0f * s, pal.border, std::max(1.0f, 1.1f * s));

            void* uiFamily = gdip::CreateBestUiFontFamily(false);
            void* iconFamily = gdip::CreateFluentIconFontFamily();

            auto drawDivider = [&](float yPos) {
                void* pen = nullptr;
                if (gdip::GdipCreatePen1(pal.borderSubtle, std::max(1.0f, 1.0f * s), gdip::kUnitPixel, &pen) == 0 && pen) {
                    gdip::GdipDrawLine(g, pen, 12.0f * s, yPos, w - 12.0f * s, yPos);
                    gdip::GdipDeletePen(pen);
                }
            };

            drawDivider(divider1Y_);
            drawDivider(divider2Y_);

            if (notifExpanded_ && !IsRectEmpty(&notifSubCardRc_)) {
                float sx = static_cast<float>(notifSubCardRc_.left);
                float sy = static_cast<float>(notifSubCardRc_.top);
                float sw = static_cast<float>(notifSubCardRc_.right - notifSubCardRc_.left);
                float sh = static_cast<float>(notifSubCardRc_.bottom - notifSubCardRc_.top);
                gdip::FillRoundedRect(g, sx, sy, sw, sh, 7.0f * s, pal.bgCardAlt);
                gdip::StrokeRoundedRect(g, sx, sy, sw, sh, 7.0f * s, pal.borderSubtle, 1.0f);

                if (isMuted_) {
                    int64_t untilSec = static_cast<int64_t>(mutedUntilMin_) * 60;
                    int dayMin = static_cast<int>((untilSec / 60) % (24 * 60));
                    if (dayMin < 0) dayMin += 24 * 60;
                    TimeHM untilHm{dayMin / 60, dayMin % 60};
                    std::wstring mutedTxt = FormatSingleArg(Tr(lang, StrId::MenuMutedUntil), untilHm.FormatHHMM());
                    gdip::DrawTextBox(
                        g, mutedTxt, uiFamily, 11.2f * s, false, pal.gold,
                        sx + 10.0f * s, sy + 37.0f * s, sw - 20.0f * s, 16.0f * s,
                        rtl ? gdip::kStringAlignmentFar : gdip::kStringAlignmentNear,
                        gdip::kStringAlignmentCenter);
                }
            }

            const int hAlign = rtl ? gdip::kStringAlignmentFar : gdip::kStringAlignmentNear;

            for (size_t i = 0; i < items_.size(); ++i) {
                const auto& item = items_[i];
                const bool active = (static_cast<int>(i) == hoverIndex_) || (static_cast<int>(i) == focusIndex_);
                const float ix = static_cast<float>(item.rc.left);
                const float iy = static_cast<float>(item.rc.top);
                const float iw = static_cast<float>(item.rc.right - item.rc.left);
                const float ih = static_cast<float>(item.rc.bottom - item.rc.top);

                if (item.action == MenuAction::HeaderFlyout) {
                    uint32_t cardBg = active ? pal.bgHighlight : pal.bgCard;
                    uint32_t cardBorder = active ? pal.borderAccentSoft : pal.borderSubtle;
                    gdip::FillRoundedRect(g, ix, iy, iw, ih, 8.0f * s, cardBg);
                    gdip::StrokeRoundedRect(g, ix, iy, iw, ih, 8.0f * s, cardBorder, 1.0f);

                    float logoSz = 28.0f * s;
                    float logoX = rtl ? (ix + iw - 10.0f * s - logoSz) : (ix + 10.0f * s);
                    float logoY = iy + (ih - logoSz) * 0.5f;
                    theme::DrawBrandLogoTile(g, logoX, logoY, logoSz);

                    if (hasState_) {
                        TimeHM nextHm{state_.nextPrayerDateTime.hour, state_.nextPrayerDateTime.minute};
                        std::wstring remTxt = FormatRemainingLong(state_.remainingSeconds, false, lang);

                        float badgeW = std::min(92.0f * s, std::max(54.0f * s, static_cast<float>(remTxt.size()) * 7.2f * s + 16.0f * s));
                        float badgeH = 22.0f * s;
                        float badgeX = rtl ? (ix + 10.0f * s) : (ix + iw - 10.0f * s - badgeW);
                        float badgeY = iy + (ih - badgeH) * 0.5f;
                        gdip::FillRoundedRect(g, badgeX, badgeY, badgeW, badgeH, 11.0f * s,
                                              isLight ? theme::WithAlpha(0x22u, pal.accent) : theme::WithAlpha(0x28u, pal.gold));
                        gdip::DrawTextBox(g, remTxt, uiFamily, 11.2f * s, true,
                                          isLight ? pal.accent : pal.gold,
                                          badgeX + 4.0f * s, badgeY, badgeW - 8.0f * s, badgeH,
                                          gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);

                        float textL = rtl ? (badgeX + badgeW + 8.0f * s) : (logoX + logoSz + 10.0f * s);
                        float textR = rtl ? (logoX - 10.0f * s) : (badgeX - 8.0f * s);
                        float textW = std::max(40.0f * s, textR - textL);

                        std::wstring topSub = std::wstring(Tr(lang, StrId::BadgeNext)) + L" · " + nextHm.FormatHHMM();
                        gdip::DrawTextBox(g, topSub, uiFamily, 10.8f * s, false, pal.textSecondary,
                                          textL, iy + 8.0f * s, textW, 15.0f * s,
                                          hAlign, gdip::kStringAlignmentCenter);

                        float dotSz = 7.0f * s;
                        float dotX = rtl ? (textR - dotSz) : textL;
                        float dotY = iy + 27.0f * s + (18.0f * s - dotSz) * 0.5f;
                        void* dotBr = nullptr;
                        if (gdip::GdipCreateSolidFill(GetPrayerAccentArgb(state_.nextPrayer), &dotBr) == 0 && dotBr) {
                            gdip::GdipFillEllipse(g, dotBr, dotX, dotY, dotSz, dotSz);
                            gdip::GdipDeleteBrush(dotBr);
                        }

                        std::wstring prayerName = GetEffectivePrayerName(state_.nextPrayer, settings_);
                        float nameX = rtl ? textL : (dotX + dotSz + 6.0f * s);
                        float nameW = std::max(30.0f * s, textW - dotSz - 6.0f * s);
                        gdip::DrawTextBox(g, prayerName, uiFamily, 13.6f * s, true, pal.textPrimary,
                                          nameX, iy + 25.0f * s, nameW, 20.0f * s,
                                          hAlign, gdip::kStringAlignmentCenter);
                    } else {
                        float textL = rtl ? (ix + 10.0f * s) : (logoX + logoSz + 10.0f * s);
                        float textW = std::max(40.0f * s, iw - logoSz - 28.0f * s);
                        gdip::DrawTextBox(g, Tr(lang, StrId::ToggleFlyout), uiFamily, 13.2f * s, true, pal.textPrimary,
                                          textL, iy, textW, ih, hAlign, gdip::kStringAlignmentCenter);
                    }
                    continue;
                }

                if (item.action == MenuAction::StopSound) {
                    uint32_t bg = active ? theme::WithAlpha(0x48u, pal.gold) : pal.amberBg;
                    gdip::FillRoundedRect(g, ix, iy, iw, ih, 6.0f * s, bg);
                    gdip::StrokeRoundedRect(g, ix, iy, iw, ih, 6.0f * s, pal.amberBorder, 1.0f);
                    float iconSz = 18.0f * s;
                    float iconX = rtl ? (ix + iw - 10.0f * s - iconSz) : (ix + 10.0f * s);
                    gdip::DrawFluentIcon(g, iconFamily, L"\uE71A", uiFamily, L"■", 12.0f * s, pal.gold,
                                         iconX, iy + (ih - iconSz) * 0.5f, iconSz, iconSz);
                    float textX = rtl ? (ix + 10.0f * s) : (iconX + iconSz + 8.0f * s);
                    float textW = std::max(30.0f * s, iw - iconSz - 28.0f * s);
                    gdip::DrawTextBox(g, Tr(lang, StrId::StopSoundMenu), uiFamily, 12.6f * s, true, pal.gold,
                                      textX, iy, textW, ih, hAlign, gdip::kStringAlignmentCenter);
                    continue;
                }

                const bool isExit = (item.action == MenuAction::Exit);
                const bool isPro = (item.action == MenuAction::Pro);

                if (active) {
                    uint32_t hoverBg = isExit
                        ? theme::WithAlpha(0x2Cu, pal.danger)
                        : (item.inSubCard ? pal.bgBtnHover : pal.bgNavHover);
                    gdip::FillRoundedRect(g, ix, iy, iw, ih, 6.0f * s, hoverBg);
                    if (!isExit && !item.inSubCard) {
                        float barW = 3.0f * s;
                        float barH = ih - 12.0f * s;
                        float barX = rtl ? (ix + iw - barW - 2.0f * s) : (ix + 2.0f * s);
                        gdip::FillRoundedRect(g, barX, iy + 6.0f * s, barW, barH, 1.5f * s, isPro ? pal.gold : pal.accent);
                    }
                } else if (isPro) {
                    gdip::FillRoundedRect(g, ix, iy, iw, ih, 6.0f * s, pal.goldSoftBg);
                }

                const wchar_t* glyph = L"";
                const wchar_t* fallback = L"•";
                std::wstring label;
                uint32_t fg = pal.textPrimary;
                uint32_t iconFg = pal.textSecondary;
                bool boldText = isPro;

                switch (item.action) {
                    case MenuAction::Imsakiye:
                        glyph = L"\uE787"; fallback = L"▦";
                        label = Tr(lang, StrId::OpenImsakiye);
                        break;
                    case MenuAction::Location:
                        glyph = L"\uE707"; fallback = L"⌖";
                        label = FormatSingleArg(Tr(lang, StrId::MenuChangeLocation), settings_.cityName);
                        break;
                    case MenuAction::Refresh:
                        glyph = L"\uE72C"; fallback = L"↻";
                        label = Tr(lang, StrId::RefreshNow);
                        break;
                    case MenuAction::NotifToggleAccordion:
                        glyph = (isMuted_ || !settings_.notificationsEnabled) ? L"\uE7ED" : L"\uEA8F";
                        fallback = L"♪";
                        label = Tr(lang, StrId::MenuNotifications);
                        if (isMuted_) iconFg = pal.gold;
                        else if (settings_.notificationsEnabled) iconFg = pal.accent;
                        break;
                    case MenuAction::NotifAllSwitch:
                        glyph = settings_.notificationsEnabled ? L"\uEA8F" : L"\uE7ED";
                        fallback = L"●";
                        label = Tr(lang, StrId::ChkNotifyAll);
                        iconFg = settings_.notificationsEnabled ? pal.accent : pal.textMuted;
                        break;
                    case MenuAction::NotifUnmute:
                        glyph = L"\uEA8F"; fallback = L"✓";
                        label = Tr(lang, StrId::MenuUnmute);
                        iconFg = pal.success;
                        break;
                    case MenuAction::NotifMute1h:
                        glyph = L"\uE823"; fallback = L"◷";
                        label = Tr(lang, StrId::MenuMute1h);
                        break;
                    case MenuAction::NotifMuteToday:
                        glyph = L"\uE708"; fallback = L"☾";
                        label = Tr(lang, StrId::MenuMuteToday);
                        break;
                    case MenuAction::NotifTest:
                        glyph = L"\uE7E7"; fallback = L"▶";
                        label = Tr(lang, StrId::BtnTestNotif);
                        break;
                    case MenuAction::Settings:
                        glyph = L"\uE713"; fallback = L"⚙";
                        label = Tr(lang, StrId::SettingsEllipsis);
                        break;
                    case MenuAction::Pro:
                        glyph = L"\uE734"; fallback = L"★";
                        label = Tr(lang, StrId::MenuUpgradePro);
                        fg = pal.gold;
                        iconFg = pal.gold;
                        break;
                    case MenuAction::Exit:
                        glyph = L"\uE7E8"; fallback = L"×";
                        label = Tr(lang, StrId::ExitApp);
                        if (active) {
                            fg = pal.danger;
                            iconFg = pal.danger;
                        }
                        break;
                    default:
                        break;
                }

                float iconPad = item.inSubCard ? 8.0f * s : 10.0f * s;
                float iconSz = 18.0f * s;
                float iconX = rtl ? (ix + iw - iconPad - iconSz) : (ix + iconPad);
                gdip::DrawFluentIcon(g, iconFamily, glyph, uiFamily, fallback, 12.6f * s, iconFg,
                                     iconX, iy + (ih - iconSz) * 0.5f, iconSz, iconSz);

                float rightReserve = 10.0f * s;
                if (item.action == MenuAction::NotifToggleAccordion) {
                    float chevSz = 14.0f * s;
                    float chevX = rtl ? (ix + 8.0f * s) : (ix + iw - 8.0f * s - chevSz);
                    gdip::DrawFluentIcon(g, iconFamily, notifExpanded_ ? L"\uE70D" : (rtl ? L"\uE76B" : L"\uE76C"),
                                         uiFamily, notifExpanded_ ? L"▾" : L"▸",
                                         9.5f * s, pal.textSecondary,
                                         chevX, iy + (ih - chevSz) * 0.5f, chevSz, chevSz);

                    float dotD = 7.0f * s;
                    float dotX = rtl ? (chevX + chevSz + 6.0f * s) : (chevX - 6.0f * s - dotD);
                    uint32_t dotCol = isMuted_ ? pal.gold : (settings_.notificationsEnabled ? pal.success : pal.textMuted);
                    void* dotBr = nullptr;
                    if (gdip::GdipCreateSolidFill(dotCol, &dotBr) == 0 && dotBr) {
                        gdip::GdipFillEllipse(g, dotBr, dotX, iy + (ih - dotD) * 0.5f, dotD, dotD);
                        gdip::GdipDeleteBrush(dotBr);
                    }
                    rightReserve = chevSz + dotD + 22.0f * s;
                } else if (item.action == MenuAction::NotifAllSwitch) {
                    float tw = 34.0f * s;
                    float th = 18.0f * s;
                    float tx = rtl ? (ix + 8.0f * s) : (ix + iw - 8.0f * s - tw);
                    theme::DrawFluentToggleSwitch(g, tx, iy + (ih - th) * 0.5f, tw, th, settings_.notificationsEnabled, pal);
                    rightReserve = tw + 16.0f * s;
                }

                float textX = rtl ? (ix + rightReserve) : (iconX + iconSz + 9.0f * s);
                float textW = std::max(30.0f * s, iw - iconPad - iconSz - 9.0f * s - rightReserve);
                gdip::DrawTextBox(g, label, uiFamily, item.inSubCard ? 12.2f * s : 12.6f * s, boldText, fg,
                                  textX, iy, textW, ih, hAlign, gdip::kStringAlignmentCenter);
            }

            if (iconFamily) gdip::GdipDeleteFontFamily(iconFamily);
            if (uiFamily) gdip::GdipDeleteFontFamily(uiFamily);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gdiBmp);
    }

    BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDc);
}

LRESULT CALLBACK ContextMenuPopup::WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ContextMenuPopup* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<ContextMenuPopup*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ContextMenuPopup*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    return self ? self->WndProc(hWnd, msg, wParam, lParam) : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT ContextMenuPopup::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

        case WM_SETCURSOR: {
            if (LOWORD(lParam) == HTCLIENT) {
                POINT pt{};
                GetCursorPos(&pt);
                ScreenToClient(hWnd, &pt);
                bool overItem = HitTestIndex(pt.x, pt.y) >= 0;
                SetCursor(LoadCursorW(nullptr, overItem ? IDC_HAND : IDC_ARROW));
                return TRUE;
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (!trackingLeave_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hWnd, 0};
                TrackMouseEvent(&tme);
                trackingLeave_ = true;
            }
            int x = static_cast<short>(LOWORD(lParam));
            int y = static_cast<short>(HIWORD(lParam));
            int nextHover = HitTestIndex(x, y);
            if (nextHover != hoverIndex_) {
                hoverIndex_ = nextHover;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            trackingLeave_ = false;
            if (hoverIndex_ != -1) {
                hoverIndex_ = -1;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP: {
            int x = static_cast<short>(LOWORD(lParam));
            int y = static_cast<short>(HIWORD(lParam));
            int hit = HitTestIndex(x, y);
            if (hit >= 0 && hit < static_cast<int>(items_.size())) {
                TriggerAction(items_[static_cast<size_t>(hit)].action);
            } else if (msg == WM_RBUTTONUP) {
                CloseNow();
            }
            return 0;
        }

        case WM_KEYDOWN: {
            const int count = static_cast<int>(items_.size());
            if (wParam == VK_ESCAPE) {
                CloseNow();
                return 0;
            }
            if (wParam == VK_DOWN && count > 0) {
                focusIndex_ = (focusIndex_ + 1 + count) % count;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (wParam == VK_UP && count > 0) {
                focusIndex_ = (focusIndex_ <= 0) ? (count - 1) : (focusIndex_ - 1);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            const bool rtl = IsRtlLanguage(settings_.language);
            const WPARAM expandKey = rtl ? VK_LEFT : VK_RIGHT;
            const WPARAM collapseKey = rtl ? VK_RIGHT : VK_LEFT;
            if (wParam == expandKey && focusIndex_ >= 0 && focusIndex_ < count) {
                MenuAction act = items_[static_cast<size_t>(focusIndex_)].action;
                if ((act == MenuAction::NotifToggleAccordion && !notifExpanded_) ||
                    act == MenuAction::NotifAllSwitch) {
                    TriggerAction(act);
                }
                return 0;
            }
            if (wParam == collapseKey && notifExpanded_) {
                notifExpanded_ = false;
                int winW = 0;
                int winH = 0;
                RebuildLayout(winW, winH);
                PositionWindow(winW, winH);
                for (size_t i = 0; i < items_.size(); ++i) {
                    if (items_[i].action == MenuAction::NotifToggleAccordion) {
                        focusIndex_ = static_cast<int>(i);
                        break;
                    }
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if ((wParam == VK_RETURN || wParam == VK_SPACE) && focusIndex_ >= 0 && focusIndex_ < count) {
                TriggerAction(items_[static_cast<size_t>(focusIndex_)].action);
                return 0;
            }
            break;
        }

        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE && !closing_) {
                CloseNow();
            }
            return 0;

        case WM_DESTROY:
            hwnd_ = nullptr;
            return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace vakit
