#include "pro_window.h"
#include "gdiplus_flat.h"
#include "../core/commercial_config.h"
#include "../core/i18n.h"
#include "../platform/theme_utils.h"
#include "../platform/win_store.h"

#include <shellapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <thread>

namespace vakit {

namespace {

constexpr wchar_t kProClassName[] = L"PrayerBarProWindow";
constexpr int kDipW = 576;
constexpr int kDipH = 432;

enum CtrlId : WORD {
    IDC_BUY_ANNUAL = 701,
    IDC_BUY_LIFETIME = 702,
    IDC_RESTORE_BTN = 703,
    IDC_MANAGE_SUB_BTN = 704,
    IDC_RATE_STORE_BTN = 705,
    IDC_CLOSE_BTN = 708
};

uint32_t WithAlpha(uint8_t a, uint32_t argb) {
    return (static_cast<uint32_t>(a) << 24) | (argb & 0x00FFFFFFu);
}

std::wstring GetControlString(HWND h) {
    int len = GetWindowTextLengthW(h);
    if (len <= 0) return {};
    std::wstring s(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    s.resize(static_cast<size_t>(len));
    return s;
}

} // namespace

ProWindow::ProWindow(
    SettingsStore& settingsStore,
    LicenseManager& licenseManager,
    std::function<void()> onLicenseChanged)
    : settingsStore_(settingsStore),
      licenseManager_(licenseManager),
      onLicenseChanged_(std::move(onLicenseChanged)) {

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kProClassName;
    RegisterClassExW(&wc);
}

ProWindow::~ProWindow() {
    CloseNow();
    if (uiFont_) DeleteObject(uiFont_);
    if (boldFont_) DeleteObject(boldFont_);
    if (smallFont_) DeleteObject(smallFont_);
    if (bgBrush_) DeleteObject(bgBrush_);
    if (cardBrush_) DeleteObject(cardBrush_);
}

void ProWindow::OpenOrFocus(bool showLockedHint) {
    showLockedHint_ = showLockedHint;
    settings_ = settingsStore_.Load();
    status_ = licenseManager_.GetStatus();

    if (hwnd_) {
        if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
        RefreshStatusTexts();
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    isDark_ = !IsAppLight(settings_.themeMode);
    ApplyWin11AppDarkMode(settings_.themeMode);
    feedbackText_.clear();
    feedbackIsError_ = false;
    busy_ = false;

    POINT pt{};
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);

    HWND tb = FindWindowW(L"Shell_TrayWnd", nullptr);
    UINT dpi = tb ? GetDpiForWindowCompat(tb) : 96;
    scale_ = dpi >= 96 ? static_cast<double>(dpi) / 96.0 : 1.0;

    int winW = static_cast<int>(std::llround(kDipW * scale_));
    int winH = static_cast<int>(std::llround(kDipH * scale_));
    int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - winW) / 2;
    int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - winH) / 2;

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    hwnd_ = CreateWindowExW(
        0,
        kProClassName,
        Tr(settings_.language, StrId::ProWindowTitle),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, winW, winH,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return;

    ApplyWindowDwmStyle(hwnd_, isDark_, 2, 2);
    RebuildFonts();
    ApplyThemeBrushes();
    CreateControls();
    RefreshStatusTexts();

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void ProWindow::CloseNow() {
    if (!hwnd_) return;
    HWND h = hwnd_;
    hwnd_ = nullptr;
    buyAnnualBtn_ = nullptr;
    buyLifetimeBtn_ = nullptr;
    restoreBtn_ = nullptr;
    manageSubBtn_ = nullptr;
    rateStoreBtn_ = nullptr;
    closeBtn_ = nullptr;
    DestroyWindow(h);
    if (settings_.autoOptimizeMemory) {
        TrimWorkingSet();
    }
}

void ProWindow::RefreshThemeOrLanguage() {
    if (!hwnd_) return;
    settings_ = settingsStore_.Load();
    status_ = licenseManager_.GetStatus();
    isDark_ = !IsAppLight(settings_.themeMode);
    ApplyWin11AppDarkMode(settings_.themeMode);
    ApplyThemeBrushes();
    SetWindowTextW(hwnd_, Tr(settings_.language, StrId::ProWindowTitle));
    RefreshStatusTexts();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ProWindow::RebuildFonts() {
    if (uiFont_) DeleteObject(uiFont_);
    if (boldFont_) DeleteObject(boldFont_);
    if (smallFont_) DeleteObject(smallFont_);
    uiFont_ = CreateWin11Font(-static_cast<int>(std::llround(13.0 * scale_)), FW_NORMAL, false);
    boldFont_ = CreateWin11Font(-static_cast<int>(std::llround(13.0 * scale_)), FW_SEMIBOLD, true);
    smallFont_ = CreateWin11Font(-static_cast<int>(std::llround(11.5 * scale_)), FW_NORMAL, false);
}

void ProWindow::ApplyThemeBrushes() {
    if (bgBrush_) DeleteObject(bgBrush_);
    if (cardBrush_) DeleteObject(cardBrush_);

    isDark_ = !IsAppLight(settings_.themeMode);
    if (isDark_) {
        bgBrush_ = CreateSolidBrush(RGB(32, 32, 32));
        cardBrush_ = CreateSolidBrush(RGB(43, 43, 46));
    } else {
        bgBrush_ = CreateSolidBrush(RGB(243, 243, 243));
        cardBrush_ = CreateSolidBrush(RGB(255, 255, 255));
    }
    if (hwnd_) ApplyWindowDwmStyle(hwnd_, isDark_, 2, 2);
}

void ProWindow::CreateControls() {
    auto S = [&](int v) -> int { return static_cast<int>(std::llround(v * scale_)); };
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    AppLanguage lang = settings_.language;

    auto mkBtn = [&](WORD id, int x, int y, int w, int h, const wchar_t* txt) -> HWND {
        HWND c = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                 S(x), S(y), S(w), S(h), hwnd_,
                                 reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), hInst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
        return c;
    };

    buyAnnualBtn_   = mkBtn(IDC_BUY_ANNUAL,     28,  154, 242, 28, Tr(lang, StrId::BtnBuyAnnual));
    buyLifetimeBtn_ = mkBtn(IDC_BUY_LIFETIME,   290, 154, 242, 28, Tr(lang, StrId::BtnBuyLifetime));

    restoreBtn_     = mkBtn(IDC_RESTORE_BTN,    16,  350, 174, 28, Tr(lang, StrId::BtnRestorePurchases));
    manageSubBtn_   = mkBtn(IDC_MANAGE_SUB_BTN, 196, 350, 134, 28, Tr(lang, StrId::BtnManageSubscription));
    rateStoreBtn_   = mkBtn(IDC_RATE_STORE_BTN, 336, 350, 114, 28, Tr(lang, StrId::BtnRateStore));
    closeBtn_       = mkBtn(IDC_CLOSE_BTN,      456, 350, 90,  28, Tr(lang, StrId::BtnOk));
}

void ProWindow::RefreshStatusTexts() {
    AppLanguage lang = settings_.language;
    if (buyAnnualBtn_) SetWindowTextW(buyAnnualBtn_, Tr(lang, StrId::BtnBuyAnnual));
    if (buyLifetimeBtn_) SetWindowTextW(buyLifetimeBtn_, Tr(lang, StrId::BtnBuyLifetime));
    if (restoreBtn_) SetWindowTextW(restoreBtn_, Tr(lang, StrId::BtnRestorePurchases));
    if (manageSubBtn_) SetWindowTextW(manageSubBtn_, Tr(lang, StrId::BtnManageSubscription));
    if (rateStoreBtn_) SetWindowTextW(rateStoreBtn_, Tr(lang, StrId::BtnRateStore));
    if (closeBtn_) SetWindowTextW(closeBtn_, Tr(lang, StrId::BtnOk));
}

void ProWindow::SetBusy(bool busy) {
    busy_ = busy;
    BOOL en = busy ? FALSE : TRUE;
    if (buyAnnualBtn_) EnableWindow(buyAnnualBtn_, en);
    if (buyLifetimeBtn_) EnableWindow(buyLifetimeBtn_, en);
    if (restoreBtn_) EnableWindow(restoreBtn_, en);
}

void ProWindow::StartStorePurchase(const wchar_t* addOnStoreId) {
    if (busy_ || !hwnd_) return;
    SetBusy(true);
    feedbackIsError_ = false;
    feedbackText_ = Tr(settings_.language, StrId::StorePurchasing);
    InvalidateRect(hwnd_, nullptr, FALSE);

    HWND target = hwnd_;
    std::wstring addonId = addOnStoreId ? addOnStoreId : L"";
    BeginMicrosoftStorePurchaseAsync(target, addonId, [target](StorePurchaseResponse resp) {
        auto* ptr = new StorePurchaseResponse(resp);
        if (!PostMessageW(target, kMsgStorePurchaseDone, 0, reinterpret_cast<LPARAM>(ptr))) {
            delete ptr;
        }
    });
}

void ProWindow::StartStoreRestore(bool /*afterPurchase*/) {
    if (!hwnd_) return;
    SetBusy(true);
    feedbackIsError_ = false;
    feedbackText_ = Tr(settings_.language, StrId::StoreRestoring);
    InvalidateRect(hwnd_, nullptr, FALSE);

    HWND target = hwnd_;
    std::thread([target]() {
        auto snap = QueryMicrosoftStoreLicenseSync();
        auto* ptr = new StoreLicenseSnapshot(snap);
        if (!PostMessageW(target, kMsgStoreRestoreDone, 0, reinterpret_cast<LPARAM>(ptr))) {
            delete ptr;
        }
    }).detach();
}

void ProWindow::PaintCanvas(HDC hdc, int width, int height) {
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

    HGDIOBJ old = SelectObject(memDc, hBmp);
    void* gdiBmp = nullptr;
    void* g = nullptr;

    if (gdip::GdipCreateBitmapFromScan0(width, height, width * 4, gdip::kPixelFormat32bppPARGB, bits, &gdiBmp) == 0 && gdiBmp) {
        if (gdip::GdipGetImageGraphicsContext(gdiBmp, &g) == 0 && g) {
            gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAliasGridFit);

            uint32_t bg = isDark_ ? 0xFF202020u : 0xFFF3F3F3u;
            uint32_t cardBg = isDark_ ? 0xFF2B2B2Eu : 0xFFFFFFFFu;
            uint32_t border = isDark_ ? 0x20FFFFFFu : 0x1A000000u;
            uint32_t textPrim = isDark_ ? 0xFFF5F7FAu : 0xFF1A1A1Au;
            uint32_t textSec = isDark_ ? 0xFFA8B0BCu : 0xFF5D6472u;
            uint32_t accent = GetWindowsAccentColorArgb(isDark_, settings_.useSystemAccentColor);
            uint32_t gold = isDark_ ? 0xFFFFCE5Eu : 0xFFB86600u;

            gdip::GdipGraphicsClear(g, bg);

            float s = static_cast<float>(scale_);
            float pad = 16.0f * s;
            float cw = static_cast<float>(width) - pad * 2.0f;

            void* uiFam = gdip::CreateBestUiFontFamily(false);
            void* dispFam = gdip::CreateBestUiFontFamily(true);
            AppLanguage lang = settings_.language;

            // Üst Başlık ve Durum Rozeti
            gdip::DrawTextBox(g, L"PrayerBar Pro", dispFam, 18.0f * s, true, textPrim,
                              pad, 10.0f * s, 210.0f * s, 26.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            std::wstring statusStr;
            uint32_t badgeCol = accent;
            switch (status_.tier) {
                case LicenseTier::ProLifetime:
                    statusStr = Tr(lang, StrId::ProStatusActiveLifetime);
                    badgeCol = 0xFF10B981u;
                    break;
                case LicenseTier::ProAnnual:
                    statusStr = Tr(lang, StrId::ProStatusActiveAnnual);
                    badgeCol = 0xFF10B981u;
                    break;
                case LicenseTier::Trial: {
                    wchar_t buf[128];
                    swprintf_s(buf, L"%ls (%d)", Tr(lang, StrId::ProStatusTrial), status_.trialDaysRemaining);
                    statusStr = buf;
                    badgeCol = accent;
                    break;
                }
                case LicenseTier::Free:
                default:
                    statusStr = Tr(lang, StrId::ProStatusFree);
                    badgeCol = gold;
                    break;
            }

            float badgeW = cw - 205.0f * s;
            gdip::FillRoundedRect(g, pad + 205.0f * s, 12.0f * s, badgeW, 24.0f * s, 6.0f * s, WithAlpha(0x28u, badgeCol));
            gdip::StrokeRoundedRect(g, pad + 205.0f * s, 12.0f * s, badgeW, 24.0f * s, 6.0f * s, WithAlpha(0x70u, badgeCol), 1.0f);
            gdip::DrawTextBox(g, statusStr, uiFam, 10.5f * s, true, badgeCol,
                              pad + 211.0f * s, 12.0f * s, badgeW - 12.0f * s, 24.0f * s,
                              gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);

            if (showLockedHint_) {
                gdip::FillRoundedRect(g, pad, 42.0f * s, cw, 24.0f * s, 5.0f * s, WithAlpha(0x22u, gold));
                gdip::DrawTextBox(g, Tr(lang, StrId::ProLockedHint), uiFam, 10.5f * s, true, gold,
                                  pad + 8.0f * s, 42.0f * s, cw - 16.0f * s, 24.0f * s,
                                  gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            } else {
                std::wstring sub = status_.expiryIso.empty()
                    ? L"Microsoft Store · Windows 11 · 6 Languages · 0% Idle CPU"
                    : (L"Microsoft Store · " + status_.expiryIso);
                gdip::DrawTextBox(g, sub, uiFam, 10.8f * s, false, textSec,
                                  pad, 42.0f * s, cw, 22.0f * s,
                                  gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            }

            // İki Microsoft Store Satın Alma Kartı
            float cardY = 74.0f * s;
            float cardH = 116.0f * s;
            float gap = 12.0f * s;
            float halfW = (cw - gap) * 0.5f;

            // Sol kart: Yıllık Abonelik
            gdip::FillRoundedRect(g, pad, cardY, halfW, cardH, 8.0f * s, cardBg);
            gdip::StrokeRoundedRect(g, pad, cardY, halfW, cardH, 8.0f * s, border, 1.0f);
            gdip::DrawTextBox(g, Tr(lang, StrId::ProPlanAnnualTitle), dispFam, 13.0f * s, true, textPrim,
                              pad + 12.0f * s, cardY + 14.0f * s, halfW - 24.0f * s, 20.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            gdip::DrawTextBox(g, Tr(lang, StrId::ProPlanAnnualPrice), dispFam, 17.5f * s, true, textPrim,
                              pad + 12.0f * s, cardY + 38.0f * s, halfW - 24.0f * s, 28.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            // Sağ kart: Ömür Boyu (Vurgulu)
            float rx = pad + halfW + gap;
            gdip::FillRoundedRect(g, rx, cardY, halfW, cardH, 8.0f * s, cardBg);
            gdip::StrokeRoundedRect(g, rx, cardY, halfW, cardH, 8.0f * s, accent, 1.5f);
            gdip::FillRoundedRect(g, rx + 12.0f * s, cardY + 8.0f * s, halfW - 24.0f * s, 16.0f * s, 4.0f * s, WithAlpha(0x28u, accent));
            gdip::DrawTextBox(g, Tr(lang, StrId::ProBadgePopular), uiFam, 8.8f * s, true, accent,
                              rx + 12.0f * s, cardY + 8.0f * s, halfW - 24.0f * s, 16.0f * s,
                              gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
            gdip::DrawTextBox(g, Tr(lang, StrId::ProPlanLifetimeTitle), dispFam, 13.0f * s, true, textPrim,
                              rx + 12.0f * s, cardY + 26.0f * s, halfW - 24.0f * s, 18.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            gdip::DrawTextBox(g, Tr(lang, StrId::ProPlanLifetimePrice), dispFam, 17.0f * s, true, accent,
                              rx + 12.0f * s, cardY + 46.0f * s, halfW - 24.0f * s, 26.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            // Pro Özellikler Listesi Kartı
            float featY = 198.0f * s;
            float featH = 120.0f * s;
            gdip::FillRoundedRect(g, pad, featY, cw, featH, 8.0f * s, cardBg);
            gdip::StrokeRoundedRect(g, pad, featY, cw, featH, 8.0f * s, border, 1.0f);
            const StrId feats[5] = {StrId::ProFeat1, StrId::ProFeat2, StrId::ProFeat3, StrId::ProFeat4, StrId::ProFeat5};
            for (int i = 0; i < 5; ++i) {
                gdip::DrawTextBox(g, Tr(lang, feats[i]), uiFam, 11.2f * s, false, textPrim,
                                  pad + 14.0f * s, featY + 8.0f * s + i * 21.0f * s, cw - 28.0f * s, 20.0f * s,
                                  gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            }

            if (!feedbackText_.empty()) {
                uint32_t fbCol = feedbackIsError_ ? 0xFFF87171u : 0xFF34D399u;
                gdip::DrawTextBox(g, feedbackText_, uiFam, 10.8f * s, true, fbCol,
                                  pad, 324.0f * s, cw, 20.0f * s,
                                  gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            }

            if (dispFam) gdip::GdipDeleteFontFamily(dispFam);
            if (uiFam) gdip::GdipDeleteFontFamily(uiFam);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gdiBmp);
    }

    BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, old);
    DeleteObject(hBmp);
    DeleteDC(memDc);
}

void ProWindow::PaintOwnerPushButton(DRAWITEMSTRUCT* dis) {
    gdip::EnsureInitialized();
    int w = dis->rcItem.right - dis->rcItem.left;
    int h = dis->rcItem.bottom - dis->rcItem.top;
    if (w <= 0 || h <= 0) return;

    WORD id = static_cast<WORD>(dis->CtlID);
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool accentBtn = (id == IDC_BUY_LIFETIME || id == IDC_CLOSE_BTN);
    bool inCard = (id == IDC_BUY_ANNUAL || id == IDC_BUY_LIFETIME);

    uint32_t accent = GetWindowsAccentColorArgb(isDark_, settings_.useSystemAccentColor);
    uint32_t parentBg = inCard ? (isDark_ ? 0xFF2B2B2Eu : 0xFFFFFFFFu) : (isDark_ ? 0xFF202020u : 0xFFF3F3F3u);

    uint32_t fill = accentBtn
        ? (disabled ? 0xFF44454Au : (pressed ? WithAlpha(0xCCu, accent) : accent))
        : (pressed ? (isDark_ ? 0xFF3F4046u : 0xFFE3E6ECu) : (isDark_ ? 0xFF34353Au : 0xFFFAFAFCu));
    uint32_t border = accentBtn ? accent : (isDark_ ? 0x26FFFFFFu : 0x22000000u);
    uint32_t fg = accentBtn
        ? (isDark_ ? 0xFF101414u : 0xFFFFFFFFu)
        : (disabled ? (isDark_ ? 0xFF7B808Bu : 0xFF9AA0AAu) : (isDark_ ? 0xFFF2F4F8u : 0xFF1A1A1Au));

    std::wstring label = GetControlString(dis->hwndItem);

    HDC memDc = CreateCompatibleDC(dis->hDC);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dis->hDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(memDc);
        return;
    }

    HGDIOBJ old = SelectObject(memDc, bmp);
    void* gBmp = nullptr;
    void* g = nullptr;
    if (gdip::GdipCreateBitmapFromScan0(w, h, w * 4, gdip::kPixelFormat32bppPARGB, bits, &gBmp) == 0 && gBmp) {
        if (gdip::GdipGetImageGraphicsContext(gBmp, &g) == 0 && g) {
            gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAliasGridFit);
            gdip::GdipGraphicsClear(g, parentBg);

            float s = static_cast<float>(scale_);
            gdip::FillRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 5.0f * s, fill);
            gdip::StrokeRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 5.0f * s, border, 1.0f);

            void* fam = gdip::CreateBestUiFontFamily(false);
            gdip::DrawTextBox(g, label, fam, 11.4f * s, accentBtn, fg,
                              4.0f * s, 0.0f, w - 8.0f * s, static_cast<float>(h),
                              gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
            if (fam) gdip::GdipDeleteFontFamily(fam);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gBmp);
    }

    BitBlt(dis->hDC, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, old);
    DeleteObject(bmp);
    DeleteDC(memDc);
}

LRESULT CALLBACK ProWindow::WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ProWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<ProWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ProWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    return self ? self->WndProc(hWnd, msg, wParam, lParam) : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT ProWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc{};
            GetClientRect(hWnd, &rc);
            PaintCanvas(hdc, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_DRAWITEM: {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (!dis) break;
            PaintOwnerPushButton(dis);
            return TRUE;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            WORD code = HIWORD(wParam);
            if (code != BN_CLICKED) break;
            if (id == IDC_BUY_ANNUAL) {
                StartStorePurchase(commercial::kStoreAddonAnnualId);
            } else if (id == IDC_BUY_LIFETIME) {
                StartStorePurchase(commercial::kStoreAddonLifetimeId);
            } else if (id == IDC_RESTORE_BTN) {
                StartStoreRestore(false);
            } else if (id == IDC_MANAGE_SUB_BTN) {
                ShellExecuteW(hWnd, L"open", commercial::kMicrosoftAccountSubsUri, nullptr, nullptr, SW_SHOWNORMAL);
            } else if (id == IDC_RATE_STORE_BTN) {
                ShellExecuteW(hWnd, L"open", commercial::kMicrosoftStoreReviewUri, nullptr, nullptr, SW_SHOWNORMAL);
            } else if (id == IDC_CLOSE_BTN) {
                CloseNow();
            }
            return 0;
        }

        case kMsgStorePurchaseDone: {
            SetBusy(false);
            auto* resp = reinterpret_cast<StorePurchaseResponse*>(lParam);
            if (resp) {
                AppLanguage lang = settings_.language;
                switch (resp->outcome) {
                    case StorePurchaseOutcome::Succeeded:
                    case StorePurchaseOutcome::AlreadyPurchased:
                        delete resp;
                        StartStoreRestore(true);
                        return 0;
                    case StorePurchaseOutcome::NotPurchased:
                        feedbackIsError_ = false;
                        feedbackText_ = Tr(lang, StrId::StorePurchaseCanceled);
                        break;
                    case StorePurchaseOutcome::NotConfigured:
                        feedbackIsError_ = true;
                        feedbackText_ = Tr(lang, StrId::StoreAddonNotConfigured);
                        break;
                    case StorePurchaseOutcome::StoreUnavailable:
                        feedbackIsError_ = true;
                        feedbackText_ = Tr(lang, StrId::StoreNotPackaged);
                        break;
                    case StorePurchaseOutcome::NetworkError:
                    case StorePurchaseOutcome::ServerError:
                    default:
                        feedbackIsError_ = true;
                        feedbackText_ = Tr(lang, StrId::StorePurchaseFailed);
                        break;
                }
                delete resp;
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case kMsgStoreRestoreDone: {
            SetBusy(false);
            auto* snap = reinterpret_cast<StoreLicenseSnapshot*>(lParam);
            if (snap) {
                AppLanguage lang = settings_.language;
                bool changed = licenseManager_.ApplyStoreLicenseSnapshot(*snap, LicenseManager::CurrentLocalDate());
                status_ = licenseManager_.GetStatus();
                if (!snap->storeAvailable) {
                    feedbackIsError_ = true;
                    feedbackText_ = Tr(lang, StrId::StoreNotPackaged);
                } else if (snap->hasProLifetime || snap->hasProAnnual) {
                    showLockedHint_ = false;
                    feedbackIsError_ = false;
                    feedbackText_ = Tr(lang, StrId::StorePurchasedOk);
                } else {
                    feedbackIsError_ = false;
                    feedbackText_ = Tr(lang, StrId::StoreRestoredNone);
                }
                delete snap;
                RefreshStatusTexts();
                if (changed && onLicenseChanged_) {
                    onLicenseChanged_();
                }
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_CLOSE:
            CloseNow();
            return 0;

        case WM_DESTROY:
            hwnd_ = nullptr;
            return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace vakit
