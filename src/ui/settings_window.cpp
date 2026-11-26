#include "settings_window.h"
#include "gdiplus_flat.h"
#include "taskbar_renderer.h"
#include "theme_palette.h"
#include "../core/city_catalog.h"
#include "../core/commercial_config.h"
#include "../core/formatter.h"
#include "../core/i18n.h"
#include "../core/prayer_calc.h"
#include "../core/utf_util.h"
#include "../platform/audio_player.h"
#include "../platform/theme_utils.h"
#include "../platform/win_store.h"

#include <commctrl.h>
#include <commdlg.h>
#include <initguid.h>
#include <oleacc.h>
#include <shellapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace vakit {

namespace {

constexpr wchar_t kSettingsClassName[] = L"PrayerBarSettingsWindow";
constexpr UINT_PTR kTimerWorldwideCitySearch = 1;
constexpr int kBaseW = 980;
constexpr int kBaseH = 836;
constexpr int kSidebarW = 214;
constexpr int kContentX = 232;
constexpr int kContentW = 720;

struct CitySearchResults {
    std::wstring query;
    std::vector<CityLocation> cities;
};

enum CtrlId : WORD {
    IDC_TAB_0 = 100,
    IDC_TAB_1 = 101,
    IDC_TAB_2 = 102,
    IDC_TAB_3 = 103,
    IDC_TAB_4 = 104,
    IDC_TAB_5 = 105,
    IDC_PREVIEW_BOX = 106,
    IDC_OK_BTN = 107,

    // Tab 0 (Görünüm)
    IDC_STYLE_COMBO = 200,
    IDC_PRESET_BASE = 201, // 201..206
    IDC_CUSTOM_EDIT = 210,
    IDC_TOKEN_BASE = 211,  // 211..217
    IDC_FLYOUT_COMBO = 218,
    IDC_FLYOUT_SIZE_SM = 233,
    IDC_FLYOUT_SIZE_MD = 234,
    IDC_FLYOUT_SIZE_LG = 235,
    IDC_FLYOUT_SCALE_SLIDER = 236,
    IDC_DOCK_COMBO = 220,
    IDC_OFFSET_SLIDER = 221,
    IDC_OFFSET_RESET = 222,
    IDC_SHOW_PILL_CHK = 223,
    IDC_OPAQUE_CHK = 224,
    IDC_SHOW_TRAY_CHK = 225,
    IDC_SHOW_BAR_CHK = 226,
    IDC_SHOW_SEC_CHK = 227,
    IDC_ACCENT_CHK = 228,
    IDC_TRAY_COMBO = 229,
    IDC_FONT_COMBO = 230,
    IDC_URGENT_COMBO = 231,
    IDC_THEME_COMBO = 232,
    IDC_TASKBAR_FRAME_CHK = 237,
    IDC_TASKBAR_LOGO_CHK = 238,

    // Tab 1 (Konum & Vakitler)
    IDC_DETECT_LOC_BTN = 300,
    IDC_AUTO_METHOD_CHK = 301,
    IDC_CITY_SEARCH_EDIT = 302,
    IDC_CITY_LIST = 303,
    IDC_CUSTOM_COORD_CHK = 304,
    IDC_LAT_EDIT = 305,
    IDC_LON_EDIT = 306,
    IDC_APPLY_COORD_BTN = 307,
    IDC_METHOD_COMBO = 308,
    IDC_ASR_COMBO = 309,
    IDC_ONLINE_SYNC_CHK = 310,
    IDC_SYNC_NOW_BTN = 311,
    IDC_OPEN_IMSAKIYE_BTN = 312,
    IDC_APPLY_OFFSETS_BTN = 313,
    IDC_AUTO_DETECT_LOC_CHK = 314,
    IDC_OFFSET_EDIT_BASE = 320, // 320..325

    // Tab 2 (Bildirim & Ses)
    IDC_ALERT_TIME_BASE = 400,  // 400..405
    IDC_ALERT_PRE_BASE = 410,   // 410..415
    IDC_ALERT_SOUND_BASE = 420, // 420..425
    IDC_ENTRY_SOUND_COMBO = 430,
    IDC_PRE_SOUND_COMBO = 431,
    IDC_CUSTOM_SOUND_EDIT = 432,
    IDC_BROWSE_SOUND_BTN = 433,
    IDC_VOLUME_SLIDER = 434,
    IDC_TEST_ENTRY_BTN = 435,
    IDC_TEST_PRE_BTN = 436,
    IDC_STOP_SOUND_BTN = 437,
    IDC_TEST_NOTIF_BTN = 438,
    IDC_QUIET_FULLSCREEN_CHK = 439,
    IDC_KERAHAT_CHK = 440,
    IDC_RAMADAN_CHK = 441,
    IDC_FRIDAY_CHK = 442,
    IDC_FAJR_SOUND_COMBO = 443,
    IDC_HOLIDAY_CHK = 444,
    IDC_EXTRA_TIMES_CHK = 445,
    IDC_TRAVEL_CHK = 446,
    IDC_TRACKING_CHK = 447,
    IDC_TOAST_CHK = 448,
    IDC_HOTKEY_CHK = 449,
    IDC_HOTKEY_BTN = 450,

    // Tab 4 (Dil & Sistem)
    IDC_LANG_COMBO = 500,
    IDC_CUSTOM_NAME_BASE = 510,  // 510..515
    IDC_CUSTOM_SHORT_BASE = 520, // 520..525
    IDC_APPLY_NAMES_BTN = 530,
    IDC_RESET_NAMES_BTN = 531,
    IDC_STARTUP_CHK = 532,
    IDC_AUTO_MEM_CHK = 533,
    IDC_EXPORT_BTN = 534,
    IDC_IMPORT_BTN = 535,
    IDC_OPEN_PRO_BTN = 536,

    // Tab 5 (Pro & Lisans)
    IDC_PRO_RESTORE_BTN = 600,
    IDC_PRO_MANAGE_BTN = 601,
    IDC_PRO_BUY_ANNUAL = 602,
    IDC_PRO_BUY_LIFETIME = 603
};

constexpr int kPreNotifyChoices[8] = {0, 5, 10, 15, 20, 30, 45, 60};
constexpr StrId kPreNotifyIds[8] = {
    StrId::PreOff, StrId::Pre5, StrId::Pre10, StrId::Pre15,
    StrId::Pre20, StrId::Pre30, StrId::Pre45, StrId::Pre60
};
constexpr int kUrgentChoices[5] = {0, 10, 15, 20, 30};
constexpr StrId kUrgentIds[5] = {
    StrId::UrgentOff, StrId::Urgent10, StrId::Urgent15, StrId::Urgent20, StrId::Urgent30
};
constexpr TaskbarDisplayStyle kQuickStyles[6] = {
    TaskbarDisplayStyle::TwoLineClassic,
    TaskbarDisplayStyle::CompactArrow,
    TaskbarDisplayStyle::CompactArrowSpaced,
    TaskbarDisplayStyle::TwoLineWithTime,
    TaskbarDisplayStyle::IconAndCountdown,
    TaskbarDisplayStyle::MinimalDotTimer
};
constexpr const wchar_t* kQuickStyleLabels[6] = {
    L"İkindiye kalan / 23 dk",
    L"İ→23Dk",
    L"İ → 23 dk",
    L"İkindi · 15:25 / 23 dk",
    L"◐ 23 dk",
    L"● 00:23"
};
constexpr const wchar_t* kTokens[7] = {
    L"{vakit}", L"{vakit_e}", L"{kisa}", L"{kalan}", L"{kalan_kisa}", L"{saat}", L"\\n"
};

struct CardBox {
    int x, y, w, h;
};

std::vector<CardBox> GetCardsForTab(int tab) {
    switch (tab) {
        case 0: return {{kContentX, 118, kContentW, 248}, {kContentX, 378, kContentW, 366}};
        case 1: return {{kContentX, 118, kContentW, 278}, {kContentX, 406, kContentW, 340}};
        case 2: return {{kContentX, 118, kContentW, 276}, {kContentX, 404, kContentW, 284}};
        case 3: return {{kContentX, 118, kContentW, 274}, {kContentX, 402, kContentW, 298}};
        case 4: return {{kContentX, 118, kContentW, 60},  {kContentX, 188, kContentW, 296}, {kContentX, 494, kContentW, 206}};
        case 5: return {{kContentX, 118, kContentW, 58},  {kContentX, 188, 353, 304}, {kContentX + 367, 188, 353, 304}, {kContentX, 504, kContentW, 190}};
        default: return {};
    }
}

std::wstring StripLeadingBullet(std::wstring s) {
    if (s.rfind(L"• ", 0) == 0) return s.substr(2);
    if (!s.empty() && s[0] == L'•') return TrimWide(s.substr(1));
    return s;
}

void SetComboSelectionByData(HWND combo, int dataVal) {
    int count = static_cast<int>(SendMessageW(combo, CB_GETCOUNT, 0, 0));
    for (int i = 0; i < count; ++i) {
        if (static_cast<int>(SendMessageW(combo, CB_GETITEMDATA, i, 0)) == dataVal) {
            SendMessageW(combo, CB_SETCURSEL, i, 0);
            return;
        }
    }
}

int GetComboSelectedData(HWND combo, int fallback) {
    int idx = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (idx < 0) return fallback;
    return static_cast<int>(SendMessageW(combo, CB_GETITEMDATA, idx, 0));
}

std::wstring GetEditText(HWND edit) {
    int len = GetWindowTextLengthW(edit);
    if (len <= 0) return {};
    std::wstring s(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(edit, s.data(), len + 1);
    s.resize(static_cast<size_t>(len));
    return s;
}

int ParseIntClamped(const std::wstring& s, int minV, int maxV, int fallback) {
    if (s.empty()) return fallback;
    wchar_t* end = nullptr;
    long v = wcstol(s.c_str(), &end, 10);
    if (!end || end == s.c_str()) return fallback;
    return std::clamp(static_cast<int>(v), minV, maxV);
}

bool ParseDoubleInvariant(std::wstring s, double& out) {
    for (wchar_t& ch : s) {
        if (ch == L',') ch = L'.';
    }
    if (s.empty()) return false;
    wchar_t* end = nullptr;
    double d = wcstod(s.c_str(), &end);
    if (!end || end == s.c_str()) return false;
    out = d;
    return true;
}

} // namespace

SettingsWindow::SettingsWindow(
    SettingsStore& settingsStore,
    ScheduleService& scheduleService,
    LicenseManager& licenseManager,
    std::function<void()> onSettingsSaved,
    std::function<void(const std::wstring&, const std::wstring&, bool)> onTestNotification,
    std::function<void()> onOpenImsakiye,
    std::function<void(bool)> onRequestUpgrade,
    std::function<bool()> isHotkeyOk,
    std::function<void()> onLicenseChanged)
    : settingsStore_(settingsStore),
      scheduleService_(scheduleService),
      licenseManager_(licenseManager),
      onSettingsSaved_(std::move(onSettingsSaved)),
      onTestNotification_(std::move(onTestNotification)),
      onOpenImsakiye_(std::move(onOpenImsakiye)),
      onRequestUpgrade_(std::move(onRequestUpgrade)),
      isHotkeyOk_(std::move(isHotkeyOk)),
      onLicenseChanged_(std::move(onLicenseChanged)) {

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kSettingsClassName;
    RegisterClassExW(&wc);
}

SettingsWindow::~SettingsWindow() {
    CloseNow();
    if (uiFont_) DeleteObject(uiFont_);
    if (boldFont_) DeleteObject(boldFont_);
    if (smallFont_) DeleteObject(smallFont_);
    if (titleFont_) DeleteObject(titleFont_);
    if (priceFont_) DeleteObject(priceFont_);
    if (bgBrush_) DeleteObject(bgBrush_);
    if (cardBrush_) DeleteObject(cardBrush_);
    if (editBrush_) DeleteObject(editBrush_);
    if (sidebarBrush_) DeleteObject(sidebarBrush_);
}

void SettingsWindow::OpenOrFocus(int tab) {
    if (tab >= 0) activeTab_ = std::clamp(tab, 0, 5);
    if (activeTab_ != 5) proLockedHint_ = false;
    if (hwnd_) {
        if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
        if (tab >= 0) SwitchTab(activeTab_);
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
        RefreshPreviewAndMemory();
        return;
    }

    settings_ = settingsStore_.Load();
    isDark_ = !IsAppLight(settings_.themeMode);
    ApplyWin11AppDarkMode(settings_.themeMode);

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    POINT pt{};
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);

    UINT dpi = GetDpiForMonitorCompat(mon);
    scale_ = dpi >= 96 ? static_cast<double>(dpi) / 96.0 : 1.0;
    double workW = static_cast<double>(std::max<LONG>(640, mi.rcWork.right - mi.rcWork.left));
    double workH = static_cast<double>(std::max<LONG>(540, mi.rcWork.bottom - mi.rcWork.top));
    scale_ = std::min(scale_, std::min((workW - 24.0) / kBaseW, (workH - 24.0) / kBaseH));

    int winW = static_cast<int>(std::llround(kBaseW * scale_));
    int winH = static_cast<int>(std::llround(kBaseH * scale_));
    int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - winW) / 2;
    int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - winH) / 2;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    hwnd_ = CreateWindowExW(
        0,
        kSettingsClassName,
        Tr(settings_.language, StrId::SettingsWindowTitle),
        style,
        x, y, winW, winH,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return;

    ApplyWindowDwmStyle(hwnd_, isDark_, 2 /*DWMWCP_ROUND*/, 2 /*DWMSBT_MAINWINDOW*/);
    RebuildFonts();
    ApplyThemeBrushes();
    CreateControls();
    PopulateAllValues();
    SwitchTab(activeTab_);

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void SettingsWindow::OpenProTab(bool showLockedHint) {
    proLockedHint_ = showLockedHint;
    proFeedbackText_.clear();
    proFeedbackIsError_ = false;
    OpenOrFocus(5);
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void SettingsWindow::DestroyAllControls() {
    if (hwnd_) KillTimer(hwnd_, kTimerWorldwideCitySearch);
    for (HWND h : allCreatedControls_) {
        if (h && IsWindow(h)) {
            DestroyWindow(h);
        }
    }
    allCreatedControls_.clear();
    for (auto& vec : tabControls_) vec.clear();
    cardStaticLabels_.clear();
    mutedStaticLabels_.clear();
    rightToggleIds_.clear();
    checkStates_.clear();
    checkInCard_.clear();
    hoveredBtn_ = nullptr;
    for (HWND& h : presetBtns_) h = nullptr;
    customTemplateLabel_ = nullptr;
    for (HWND& h : tokenBtns_) h = nullptr;
    proRestoreBtn_ = nullptr;
    proManageBtn_ = nullptr;
    proBuyAnnualBtn_ = nullptr;
    proBuyLifetimeBtn_ = nullptr;
}

void SettingsWindow::CloseNow() {
    if (hwnd_) {
        HWND h = hwnd_;
        hwnd_ = nullptr;
        allCreatedControls_.clear();
        for (auto& vec : tabControls_) vec.clear();
        cardStaticLabels_.clear();
        mutedStaticLabels_.clear();
        rightToggleIds_.clear();
        checkStates_.clear();
        checkInCard_.clear();
        hoveredBtn_ = nullptr;
        for (HWND& p : presetBtns_) p = nullptr;
        customTemplateLabel_ = nullptr;
        for (HWND& t : tokenBtns_) t = nullptr;
        proRestoreBtn_ = nullptr;
        proManageBtn_ = nullptr;
        proBuyAnnualBtn_ = nullptr;
        proBuyLifetimeBtn_ = nullptr;
        DestroyWindow(h);
        if (settings_.autoOptimizeMemory) {
            TrimWorkingSet();
        }
    }
}

void SettingsWindow::RebuildUiAfterChange() {
    if (!hwnd_) return;
    settings_ = settingsStore_.Load();
    isDark_ = !IsAppLight(settings_.themeMode);
    ApplyWin11AppDarkMode(settings_.themeMode);
    ApplyThemeBrushes();
    RebuildFonts();
    SetWindowTextW(hwnd_, Tr(settings_.language, StrId::SettingsWindowTitle));
    DestroyAllControls();
    CreateControls();
    PopulateAllValues();
    SwitchTab(activeTab_);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void SettingsWindow::RefreshPreviewAndMemory() {
    if (!hwnd_) return;
    AppSettings fresh = settingsStore_.Load();
    if (fresh.flyoutScalePercent != settings_.flyoutScalePercent) {
        settings_.flyoutScalePercent = fresh.flyoutScalePercent;
        if (flyoutScaleSlider_) {
            isLoading_ = true;
            SendMessageW(flyoutScaleSlider_, TBM_SETPOS, TRUE, settings_.flyoutScalePercent);
            wchar_t fsBuf[16];
            swprintf_s(fsBuf, L"%%%d", settings_.flyoutScalePercent);
            if (flyoutScaleLabel_) SetWindowTextW(flyoutScaleLabel_, fsBuf);
            isLoading_ = false;
        }
    }
    if (previewBox_) InvalidateRect(previewBox_, nullptr, FALSE);
    if (telemetryLabel_) {
        MemoryStats st = GetMemoryStats();
        wchar_t buf[160];
        swprintf_s(buf, L"Working Set: %.1f MB  |  Private: %.1f MB  |  Handles: %lu",
                   st.workingSetMb, st.privateMb, st.handleCount);
        SetWindowTextW(telemetryLabel_, buf);
    }
    RefreshLicenseStatus();
}

void SettingsWindow::RefreshLicenseStatus() {
    if (!hwnd_) return;
    LicenseStatus lst = licenseManager_.GetStatus();
    AppLanguage lang = settings_.language;
    if (licenseLabel_) {
        wchar_t buf[192]{};
        if (lst.tier == LicenseTier::ProLifetime) {
            swprintf_s(buf, L"%ls %ls", Tr(lang, StrId::SectionLicense), Tr(lang, StrId::ProStatusActiveLifetime));
        } else if (lst.tier == LicenseTier::ProAnnual) {
            swprintf_s(buf, L"%ls %ls", Tr(lang, StrId::SectionLicense), Tr(lang, StrId::ProStatusActiveAnnual));
        } else if (lst.tier == LicenseTier::Trial) {
            swprintf_s(buf, L"%ls %ls (%d)", Tr(lang, StrId::SectionLicense), Tr(lang, StrId::ProStatusTrial), lst.trialDaysRemaining);
        } else {
            swprintf_s(buf, L"%ls %ls", Tr(lang, StrId::SectionLicense), Tr(lang, StrId::ProStatusFree));
        }
        SetWindowTextW(licenseLabel_, buf);
    }
    if (activeTab_ == 5) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void SettingsWindow::RebuildFonts() {
    if (uiFont_) DeleteObject(uiFont_);
    if (boldFont_) DeleteObject(boldFont_);
    if (smallFont_) DeleteObject(smallFont_);
    if (titleFont_) DeleteObject(titleFont_);
    if (priceFont_) DeleteObject(priceFont_);

    int hUi = -static_cast<int>(std::llround(14.0 * scale_));
    int hSmall = -static_cast<int>(std::llround(12.5 * scale_));
    int hTitle = -static_cast<int>(std::llround(21.0 * scale_));
    int hPrice = -static_cast<int>(std::llround(24.0 * scale_));
    uiFont_ = CreateWin11Font(hUi, FW_NORMAL, false);
    boldFont_ = CreateWin11Font(hUi, FW_SEMIBOLD, true);
    smallFont_ = CreateWin11Font(hSmall, FW_NORMAL, false);
    titleFont_ = CreateWin11Font(hTitle, FW_BOLD, true);
    priceFont_ = CreateWin11Font(hPrice, FW_BOLD, true);
}

void SettingsWindow::ApplyThemeBrushes() {
    if (bgBrush_) DeleteObject(bgBrush_);
    if (cardBrush_) DeleteObject(cardBrush_);
    if (editBrush_) DeleteObject(editBrush_);
    if (sidebarBrush_) DeleteObject(sidebarBrush_);

    isDark_ = !IsAppLight(settings_.themeMode);
    auto pal = theme::GetUiPalette(isDark_, settings_.useSystemAccentColor);
    bgBrush_ = CreateSolidBrush(theme::ArgbToColorRef(pal.bgWindow));
    cardBrush_ = CreateSolidBrush(theme::ArgbToColorRef(pal.bgCard));
    editBrush_ = CreateSolidBrush(theme::ArgbToColorRef(pal.bgInput));
    sidebarBrush_ = CreateSolidBrush(theme::ArgbToColorRef(pal.bgSidebar));
    if (hwnd_) {
        ApplyWindowDwmStyle(hwnd_, isDark_, 2, 2);
    }
}

namespace {
void AnnotateAccessible(HWND h, long role, long state) {
    static IAccPropServices* svc = [] {
        IAccPropServices* p = nullptr;
        CoCreateInstance(CLSID_AccPropServices, nullptr, CLSCTX_INPROC_SERVER, IID_IAccPropServices, reinterpret_cast<void**>(&p));
        return p;
    }();
    if (!svc || !h) return;
    VARIANT v;
    VariantInit(&v);
    v.vt = VT_I4;
    v.lVal = role;
    svc->SetHwndProp(h, static_cast<DWORD>(OBJID_CLIENT), CHILDID_SELF, PROPID_ACC_ROLE, v);
    v.lVal = state;
    svc->SetHwndProp(h, static_cast<DWORD>(OBJID_CLIENT), CHILDID_SELF, PROPID_ACC_STATE, v);
}
} // namespace

LRESULT CALLBACK SettingsWindow::HoverBtnProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) {
    auto* self = reinterpret_cast<SettingsWindow*>(ref);
    if (!self) return DefSubclassProc(h, msg, wp, lp);
    if (msg == WM_MOUSEMOVE) {
        if (self->hoveredBtn_ != h) {
            HWND prev = self->hoveredBtn_;
            self->hoveredBtn_ = h;
            if (prev && IsWindow(prev)) InvalidateRect(prev, nullptr, FALSE);
            InvalidateRect(h, nullptr, FALSE);
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, h, 0};
            TrackMouseEvent(&tme);
        }
    } else if (msg == WM_MOUSELEAVE) {
        if (self->hoveredBtn_ == h) {
            self->hoveredBtn_ = nullptr;
            InvalidateRect(h, nullptr, FALSE);
        }
    }
    return DefSubclassProc(h, msg, wp, lp);
}

void SettingsWindow::SetFluentCheck(WORD id, HWND h, bool checked) {
    checkStates_[id] = checked;
    if (h && IsWindow(h)) {
        AnnotateAccessible(h, ROLE_SYSTEM_CHECKBUTTON, STATE_SYSTEM_FOCUSABLE | (checked ? STATE_SYSTEM_CHECKED : 0));
        InvalidateRect(h, nullptr, FALSE);
    }
}

bool SettingsWindow::GetFluentCheck(WORD id) const {
    auto it = checkStates_.find(id);
    return it != checkStates_.end() && it->second;
}

bool SettingsWindow::IsControlInCard(WORD id) const {
    auto it = checkInCard_.find(id);
    return it != checkInCard_.end() && it->second;
}

bool SettingsWindow::IsAccentButton(WORD id) const {
    return id == IDC_OK_BTN || id == IDC_DETECT_LOC_BTN || id == IDC_SYNC_NOW_BTN ||
           id == IDC_OPEN_IMSAKIYE_BTN || id == IDC_APPLY_NAMES_BTN || id == IDC_APPLY_COORD_BTN ||
           id == IDC_OPEN_PRO_BTN || id == IDC_PRO_BUY_LIFETIME;
}

void SettingsWindow::CreateControls() {
    auto S = [&](int v) -> int { return static_cast<int>(std::llround(v * scale_)); };
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    AppLanguage lang = settings_.language;

    auto isInTabCard = [&](int tab, int x, int y, int w, int h) -> bool {
        if (tab < 0 || tab > 5) return false;
        int cx = x + w / 2;
        int cy = y + h / 2;
        for (const auto& box : GetCardsForTab(tab)) {
            if (cx >= box.x && cx <= (box.x + box.w) && cy >= box.y && cy <= (box.y + box.h)) {
                return true;
            }
        }
        return false;
    };

    auto applyCtrlTheme = [&](HWND h, bool isCombo) {
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
        if (isDark_) {
            SetWindowTheme(h, isCombo ? L"DarkMode_CFD" : L"DarkMode_Explorer", nullptr);
        } else {
            SetWindowTheme(h, L"Explorer", nullptr);
        }
    };

    auto track = [&](int tab, HWND c) {
        allCreatedControls_.push_back(c);
        if (tab >= 0 && tab < 6) tabControls_[tab].push_back(c);
    };

    auto makeLabel = [&](int tab, int x, int y, int w, int h, const wchar_t* txt, bool bold = false, bool isSmall = false) -> HWND {
        HWND c = CreateWindowExW(0, L"STATIC", txt, WS_CHILD | SS_LEFT | SS_NOPREFIX,
                                 S(x), S(y), S(w), S(h), hwnd_, nullptr, hInst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? boldFont_ : (isSmall ? smallFont_ : uiFont_)), TRUE);
        if (isInTabCard(tab, x, y, w, h)) cardStaticLabels_.insert(c);
        if (isSmall && !bold) mutedStaticLabels_.insert(c);
        track(tab, c);
        return c;
    };

    auto makeBtn = [&](int tab, WORD id, int x, int y, int w, int h, const wchar_t* txt) -> HWND {
        HWND c = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                                 S(x), S(y), S(w), S(h),
                                 hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), hInst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
        SetWindowSubclass(c, HoverBtnProc, 2, reinterpret_cast<DWORD_PTR>(this));
        checkInCard_[id] = isInTabCard(tab, x, y, w, h);
        track(tab, c);
        return c;
    };

    auto makeChk = [&](int tab, WORD id, int x, int y, int w, int h, const wchar_t* txt, bool rightToggle = false) -> HWND {
        HWND c = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                                 S(x), S(y), S(w), S(h),
                                 hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), hInst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
        SetWindowSubclass(c, HoverBtnProc, 2, reinterpret_cast<DWORD_PTR>(this));
        checkStates_[id] = false;
        checkInCard_[id] = isInTabCard(tab, x, y, w, h);
        if (rightToggle) rightToggleIds_.insert(id);
        track(tab, c);
        return c;
    };

    auto makeCombo = [&](int tab, WORD id, int x, int y, int w, int dropH) -> HWND {
        HWND c = CreateWindowExW(0, L"COMBOBOX", L"",
                                 WS_CHILD | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                 S(x), S(y), S(w), S(dropH),
                                 hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), hInst, nullptr);
        applyCtrlTheme(c, true);
        track(tab, c);
        return c;
    };

    auto makeEdit = [&](int tab, WORD id, int x, int y, int w, int h, bool readOnly = false) -> HWND {
        DWORD st = WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL | (readOnly ? ES_READONLY : 0);
        HWND c = CreateWindowExW(0, L"EDIT", L"", st, S(x), S(y), S(w), S(h),
                                 hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), hInst, nullptr);
        applyCtrlTheme(c, false);
        track(tab, c);
        return c;
    };

    // Sol kenar çubuğu (6 sekme) + sağ üst canlı önizleme + alt bilgi çubuğu
    previewBox_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                  S(kContentX), S(14), S(kContentW), S(58), hwnd_,
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_PREVIEW_BOX)), hInst, nullptr);
    allCreatedControls_.push_back(previewBox_);

    tabBtns_[0] = makeBtn(-1, IDC_TAB_0, 12,  68, 190, 42, Tr(lang, StrId::TabAppearance));
    tabBtns_[1] = makeBtn(-1, IDC_TAB_1, 12, 114, 190, 42, Tr(lang, StrId::TabLocation));
    tabBtns_[2] = makeBtn(-1, IDC_TAB_2, 12, 160, 190, 42, Tr(lang, StrId::TabNotifications));
    tabBtns_[3] = makeBtn(-1, IDC_TAB_3, 12, 206, 190, 42, Tr(lang, StrId::TabFeatures));
    tabBtns_[4] = makeBtn(-1, IDC_TAB_4, 12, 252, 190, 42, Tr(lang, StrId::TabLanguageSystem));
    tabBtns_[5] = makeBtn(-1, IDC_TAB_5, 12, 314, 190, 42, Tr(lang, StrId::TabProLicense));
    for (HWND b : tabBtns_) ShowWindow(b, SW_SHOW);

    footerInfo_ = makeLabel(-1, kContentX, 768, 560, 22, Tr(lang, StrId::FooterAutoSave), false, true);
    ShowWindow(footerInfo_, SW_SHOW);
    okBtn_ = makeBtn(-1, IDC_OK_BTN, kContentX + kContentW - 116, 762, 116, 34, Tr(lang, StrId::BtnOk));
    ShowWindow(okBtn_, SW_SHOW);

    // ==================== TAB 0: GÖRÜNÜM ====================
    constexpr int kL = kContentX + 18;   // 250
    constexpr int kR = kContentX + 220;  // 452
    constexpr int kRW = 482;

    makeLabel(0, kL, 130, 192, 24, Tr(lang, StrId::LblTaskbarStyle), true);
    styleCombo_ = makeCombo(0, IDC_STYLE_COMBO, kR, 127, kRW, 240);

    for (int i = 0; i < 6; ++i) {
        int col = i % 3;
        int row = i / 3;
        presetBtns_[i] = makeBtn(0, static_cast<WORD>(IDC_PRESET_BASE + i), kL + col * 231, 166 + row * 32, 222, 28, kQuickStyleLabels[i]);
    }

    customTemplateLabel_ = makeLabel(0, kL, 170, 192, 24, Tr(lang, StrId::LblCustomTemplate));
    customEdit_ = makeEdit(0, IDC_CUSTOM_EDIT, kR, 167, kRW, 26);

    int tx = kR;
    const int tokW[7] = {64, 78, 60, 68, 86, 62, 40};
    for (int i = 0; i < 7; ++i) {
        tokenBtns_[i] = makeBtn(0, static_cast<WORD>(IDC_TOKEN_BASE + i), tx, 200, tokW[i], 25, kTokens[i]);
        tx += tokW[i] + 4;
    }

    makeLabel(0, kL, 244, 192, 24, Tr(lang, StrId::LblFlyoutDesign), true);
    flyoutCombo_ = makeCombo(0, IDC_FLYOUT_COMBO, kR, 241, kRW, 180);

    makeLabel(0, kL, 282, 192, 24, Tr(lang, StrId::LblFlyoutSize), true);
    makeBtn(0, IDC_FLYOUT_SIZE_SM, kR,       279, 72, 27, Tr(lang, StrId::FlyoutSizeSmall));
    makeBtn(0, IDC_FLYOUT_SIZE_MD, kR + 78,  279, 76, 27, Tr(lang, StrId::FlyoutSizeNormal));
    makeBtn(0, IDC_FLYOUT_SIZE_LG, kR + 160, 279, 72, 27, Tr(lang, StrId::FlyoutSizeLarge));
    flyoutScaleSlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_TABSTOP | TBS_HORZ,
                                         S(kR + 240), S(279), S(186), S(26), hwnd_,
                                         reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_FLYOUT_SCALE_SLIDER)), hInst, nullptr);
    SendMessageW(flyoutScaleSlider_, TBM_SETRANGEMIN, FALSE, 80);
    SendMessageW(flyoutScaleSlider_, TBM_SETRANGEMAX, FALSE, 200);
    SendMessageW(flyoutScaleSlider_, TBM_SETLINESIZE, 0, 5);
    SendMessageW(flyoutScaleSlider_, TBM_SETPAGESIZE, 0, 15);
    track(0, flyoutScaleSlider_);
    flyoutScaleLabel_ = makeLabel(0, kR + 430, 282, 52, 22, L"%120");

    makeLabel(0, kL, 320, 192, 24, Tr(lang, StrId::LblTheme), true);
    themeCombo_ = makeCombo(0, IDC_THEME_COMBO, kR, 317, kRW, 150);

    makeLabel(0, kL, 392, 192, 24, Tr(lang, StrId::LblDockPos), true);
    dockCombo_ = makeCombo(0, IDC_DOCK_COMBO, kR, 389, kRW, 160);

    makeLabel(0, kL, 428, 192, 24, Tr(lang, StrId::LblOffset));
    offsetSlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_TABSTOP | TBS_HORZ,
                                    S(kR), S(426), S(368), S(26), hwnd_,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OFFSET_SLIDER)), hInst, nullptr);
    SendMessageW(offsetSlider_, TBM_SETRANGEMIN, FALSE, -500);
    SendMessageW(offsetSlider_, TBM_SETRANGEMAX, FALSE, 500);
    SendMessageW(offsetSlider_, TBM_SETLINESIZE, 0, 5);
    SendMessageW(offsetSlider_, TBM_SETPAGESIZE, 0, 25);
    track(0, offsetSlider_);
    offsetLabel_ = makeLabel(0, kR + 374, 429, 56, 22, L"0 px");
    makeBtn(0, IDC_OFFSET_RESET, kR + 436, 426, 46, 25, L"0");

    showPillChk_     = makeChk(0, IDC_SHOW_PILL_CHK,     kL,       466, 336, 24, Tr(lang, StrId::ChkShowPill));
    showTrayChk_     = makeChk(0, IDC_SHOW_TRAY_CHK,     kL + 348, 466, 336, 24, Tr(lang, StrId::ChkShowTray));
    taskbarFrameChk_ = makeChk(0, IDC_TASKBAR_FRAME_CHK, kL,       498, 336, 24, Tr(lang, StrId::ChkTaskbarFrame));
    taskbarLogoChk_  = makeChk(0, IDC_TASKBAR_LOGO_CHK,  kL + 348, 498, 336, 24, Tr(lang, StrId::ChkTaskbarLogo));
    showBarChk_      = makeChk(0, IDC_SHOW_BAR_CHK,      kL,       530, 336, 24, Tr(lang, StrId::ChkShowBar));
    showSecChk_      = makeChk(0, IDC_SHOW_SEC_CHK,      kL + 348, 530, 336, 24, Tr(lang, StrId::ChkShowSec));
    opaqueChk_       = makeChk(0, IDC_OPAQUE_CHK,        kL,       562, 336, 24, Tr(lang, StrId::ChkOpaqueBg));
    accentChk_       = makeChk(0, IDC_ACCENT_CHK,        kL + 348, 562, 336, 24, Tr(lang, StrId::ChkAccentSync));

    makeLabel(0, kL, 606, 192, 24, Tr(lang, StrId::LblTrayStyle));
    trayCombo_ = makeCombo(0, IDC_TRAY_COMBO, kR, 603, kRW, 150);

    makeLabel(0, kL, 642, 192, 24, Tr(lang, StrId::LblFontSize));
    fontCombo_ = makeCombo(0, IDC_FONT_COMBO, kR, 639, kRW, 150);

    makeLabel(0, kL, 678, 192, 24, Tr(lang, StrId::LblUrgentColor));
    urgentCombo_ = makeCombo(0, IDC_URGENT_COMBO, kR, 675, kRW, 170);

    // ==================== TAB 1: KONUM & VAKİTLER ====================
    detectLocBtn_ = makeBtn(1, IDC_DETECT_LOC_BTN, kL, 128, 196, 30, Tr(lang, StrId::BtnDetectLocation));
    autoLocStatus_ = makeLabel(1, kL + 206, 132, 478, 24, Tr(lang, StrId::LocDetectHint), false, true);
    autoDetectLocChk_ = makeChk(1, IDC_AUTO_DETECT_LOC_CHK, kL, 160, 684, 24, Tr(lang, StrId::ChkAutoDetectLocation));
    autoMethodChk_ = makeChk(1, IDC_AUTO_METHOD_CHK, kL, 188, 684, 24, Tr(lang, StrId::ChkAutoMethod));

    makeLabel(1, kL, 222, 192, 24, Tr(lang, StrId::LblCitySearch), true);
    citySearchEdit_ = makeEdit(1, IDC_CITY_SEARCH_EDIT, kR, 219, kRW, 26);

    cityList_ = CreateWindowExW(0, L"LISTBOX", L"",
                                WS_CHILD | WS_TABSTOP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                S(kL), S(252), S(684), S(98),
                                hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_CITY_LIST)), hInst, nullptr);
    applyCtrlTheme(cityList_, false);
    track(1, cityList_);

    customCoordChk_ = makeChk(1, IDC_CUSTOM_COORD_CHK, kL, 358, 180, 24, Tr(lang, StrId::ChkCustomCoord));
    makeLabel(1, kL + 190, 360, 56, 22, Tr(lang, StrId::LblLat));
    latEdit_ = makeEdit(1, IDC_LAT_EDIT, kL + 248, 357, 102, 25);
    makeLabel(1, kL + 362, 360, 56, 22, Tr(lang, StrId::LblLon));
    lonEdit_ = makeEdit(1, IDC_LON_EDIT, kL + 420, 357, 102, 25);
    applyCoordBtn_ = makeBtn(1, IDC_APPLY_COORD_BTN, kL + 534, 356, 150, 27, Tr(lang, StrId::BtnApplyCoord));

    makeLabel(1, kL, 422, 192, 24, Tr(lang, StrId::LblCalcMethod), true);
    methodCombo_ = makeCombo(1, IDC_METHOD_COMBO, kR, 419, kRW, 280);

    makeLabel(1, kL, 458, 192, 24, Tr(lang, StrId::LblAsrMethod));
    asrCombo_ = makeCombo(1, IDC_ASR_COMBO, kR, 455, kRW, 140);

    onlineSyncChk_ = makeChk(1, IDC_ONLINE_SYNC_CHK, kL, 494, 684, 24, Tr(lang, StrId::ChkOnlineSync));
    syncBtn_ = makeBtn(1, IDC_SYNC_NOW_BTN, kL, 526, 200, 30, Tr(lang, StrId::BtnSyncNow));
    openImsakiyeBtn_ = makeBtn(1, IDC_OPEN_IMSAKIYE_BTN, kL + 210, 526, 200, 30, Tr(lang, StrId::BtnOpenImsakiye));
    syncStatus_ = makeLabel(1, kL, 564, 684, 22, L"", false, true);

    makeLabel(1, kL, 594, 420, 22, Tr(lang, StrId::LblOffsets), true);
    makeBtn(1, IDC_APPLY_OFFSETS_BTN, kL + 534, 590, 150, 27, Tr(lang, StrId::BtnApplyOffsets));
    for (int i = 0; i < 6; ++i) {
        int col = i % 3;
        int row = i / 3;
        int bx = kL + col * 232;
        int by = 626 + row * 36;
        makeLabel(1, bx, by + 3, 92, 22, GetEffectivePrayerName(static_cast<PrayerKind>(i), settings_).c_str());
        offsetEdits_[i] = makeEdit(1, static_cast<WORD>(IDC_OFFSET_EDIT_BASE + i), bx + 98, by, 86, 26);
    }

    // ==================== TAB 2: BİLDİRİM & SES ====================
    makeLabel(2, kL, 128, 420, 24, Tr(lang, StrId::SectionPrayerAlerts), true);
    makeLabel(2, kL, 156, 130, 20, Tr(lang, StrId::ColPrayer), false, true);
    makeLabel(2, kL + 140, 156, 128, 20, Tr(lang, StrId::ColAtTime), false, true);
    makeLabel(2, kL + 280, 156, 232, 20, Tr(lang, StrId::ColPreRemind), false, true);
    makeLabel(2, kL + 528, 156, 150, 20, Tr(lang, StrId::ColSoundEzan), false, true);

    for (int i = 0; i < 6; ++i) {
        int ry = 180 + i * 33;
        makeLabel(2, kL, ry + 3, 130, 22, GetEffectivePrayerName(static_cast<PrayerKind>(i), settings_).c_str(), true);
        alertAtTimeChk_[i] = makeChk(2, static_cast<WORD>(IDC_ALERT_TIME_BASE + i), kL + 140, ry + 1, 128, 24, Tr(lang, StrId::ChkNotify));
        alertPreCombo_[i] = makeCombo(2, static_cast<WORD>(IDC_ALERT_PRE_BASE + i), kL + 280, ry - 1, 232, 220);
        alertSoundChk_[i] = makeChk(2, static_cast<WORD>(IDC_ALERT_SOUND_BASE + i), kL + 528, ry + 1, 150, 24, Tr(lang, StrId::ChkPlaySound));
    }

    makeLabel(2, kL, 416, 420, 24, Tr(lang, StrId::SectionSounds), true);
    makeLabel(2, kL, 448, 192, 24, Tr(lang, StrId::LblEntrySound));
    entrySoundCombo_ = makeCombo(2, IDC_ENTRY_SOUND_COMBO, kR, 445, kRW, 190);
    makeLabel(2, kL, 484, 192, 24, Tr(lang, StrId::LblFajrSound));
    fajrSoundCombo_ = makeCombo(2, IDC_FAJR_SOUND_COMBO, kR, 481, kRW, 210);
    makeLabel(2, kL, 520, 192, 24, Tr(lang, StrId::LblPreSound));
    preSoundCombo_ = makeCombo(2, IDC_PRE_SOUND_COMBO, kR, 517, kRW, 190);

    makeLabel(2, kL, 556, 192, 24, Tr(lang, StrId::LblCustomSound));
    customSoundEdit_ = makeEdit(2, IDC_CUSTOM_SOUND_EDIT, kR, 553, 376, 26, true);
    browseSoundBtn_ = makeBtn(2, IDC_BROWSE_SOUND_BTN, kR + 384, 552, 98, 28, Tr(lang, StrId::BtnBrowse));

    makeLabel(2, kL, 592, 192, 24, Tr(lang, StrId::LblVolume));
    volumeSlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_TABSTOP | TBS_HORZ,
                                    S(kR), S(590), S(416), S(28), hwnd_,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_VOLUME_SLIDER)), hInst, nullptr);
    SendMessageW(volumeSlider_, TBM_SETRANGEMIN, FALSE, 0);
    SendMessageW(volumeSlider_, TBM_SETRANGEMAX, FALSE, 100);
    track(2, volumeSlider_);
    volumeLabel_ = makeLabel(2, kR + 424, 593, 58, 22, L"%80");

    makeBtn(2, IDC_TEST_ENTRY_BTN, kL,       636, 162, 32, Tr(lang, StrId::BtnTestEntry));
    makeBtn(2, IDC_TEST_PRE_BTN,   kL + 170, 636, 162, 32, Tr(lang, StrId::BtnTestPre));
    makeBtn(2, IDC_STOP_SOUND_BTN, kL + 340, 636, 146, 32, Tr(lang, StrId::BtnStopSound));
    makeBtn(2, IDC_TEST_NOTIF_BTN, kL + 494, 636, 190, 32, Tr(lang, StrId::BtnTestNotif));

    // ==================== TAB 3: ÖZELLİKLER ====================
    auto feature = [&](WORD id, int y, StrId title, StrId desc, bool hasDesc) -> HWND {
        HWND c = makeChk(3, id, kL, y, 684, 24, Tr(lang, title), true);
        if (hasDesc) makeLabel(3, kL + 4, y + 24, 616, 19, Tr(lang, desc), false, true);
        return c;
    };
    makeLabel(3, kL, 128, 460, 24, Tr(lang, StrId::SectionReminders), true);
    fridayChk_ = feature(IDC_FRIDAY_CHK, 158, StrId::ChkFridayReminder, StrId::ChkFridayReminder, false);
    holidayChk_ = feature(IDC_HOLIDAY_CHK, 194, StrId::ChkHolidays, StrId::DescHolidays, true);
    travelChk_ = feature(IDC_TRAVEL_CHK, 244, StrId::ChkTravelMode, StrId::DescTravelMode, true);
    toastChk_ = feature(IDC_TOAST_CHK, 294, StrId::ChkModernToast, StrId::DescModernToast, true);
    quietFullscreenChk_ = feature(IDC_QUIET_FULLSCREEN_CHK, 344, StrId::ChkQuietFullscreen, StrId::ChkQuietFullscreen, false);

    makeLabel(3, kL, 414, 460, 24, Tr(lang, StrId::SectionDisplayTracking), true);
    kerahatChk_ = feature(IDC_KERAHAT_CHK, 444, StrId::ChkKerahatIndicator, StrId::ChkKerahatIndicator, false);
    ramadanChk_ = feature(IDC_RAMADAN_CHK, 480, StrId::ChkRamadanMode, StrId::ChkRamadanMode, false);
    extraTimesChk_ = feature(IDC_EXTRA_TIMES_CHK, 516, StrId::ChkExtraTimes, StrId::DescExtraTimes, true);
    trackingChk_ = feature(IDC_TRACKING_CHK, 566, StrId::ChkTracking, StrId::DescTracking, true);
    hotkeyChk_ = makeChk(3, IDC_HOTKEY_CHK, kL, 618, 684, 26, Tr(lang, StrId::ChkHotkey), true);
    hotkeyBtn_ = makeBtn(3, IDC_HOTKEY_BTN, kL + 470, 617, 158, 28, L"");
    hotkeyDesc_ = makeLabel(3, kL + 4, 646, 616, 20, Tr(lang, StrId::DescHotkey), false, true);
    capturingHotkey_ = false;
    SetWindowSubclass(hotkeyBtn_, HotkeyBtnProc, 1, reinterpret_cast<DWORD_PTR>(this));

    // ==================== TAB 4: DİL & SİSTEM ====================
    makeLabel(4, kL, 136, 192, 24, Tr(lang, StrId::LblAppLanguage), true);
    langCombo_ = makeCombo(4, IDC_LANG_COMBO, kR, 133, kRW, 220);

    makeLabel(4, kL, 200, 684, 22, Tr(lang, StrId::SectionCustomNames), true);
    makeLabel(4, kL, 228, 160, 20, Tr(lang, StrId::ColDefaultName), false, true);
    makeLabel(4, kL + 180, 228, 280, 20, Tr(lang, StrId::ColCustomName), false, true);
    makeLabel(4, kL + 480, 228, 140, 20, Tr(lang, StrId::ColShortCode), false, true);

    for (int i = 0; i < 6; ++i) {
        int ry = 252 + i * 32;
        makeLabel(4, kL, ry + 3, 170, 22, GetDefaultPrayerName(static_cast<PrayerKind>(i), lang), true);
        customNameEdits_[i] = makeEdit(4, static_cast<WORD>(IDC_CUSTOM_NAME_BASE + i), kL + 180, ry, 280, 26);
        customShortEdits_[i] = makeEdit(4, static_cast<WORD>(IDC_CUSTOM_SHORT_BASE + i), kL + 480, ry, 120, 26);
    }

    makeBtn(4, IDC_APPLY_NAMES_BTN, kL, 444, 190, 30, Tr(lang, StrId::BtnApplyNames));
    makeBtn(4, IDC_RESET_NAMES_BTN, kL + 200, 444, 190, 30, Tr(lang, StrId::BtnResetNames));

    makeLabel(4, kL, 506, 684, 22, Tr(lang, StrId::SectionSystem), true);
    startupChk_ = makeChk(4, IDC_STARTUP_CHK, kL, 532, 684, 24, Tr(lang, StrId::ChkStartup));
    autoMemChk_ = makeChk(4, IDC_AUTO_MEM_CHK, kL, 560, 684, 24, Tr(lang, StrId::ChkAutoMem));
    telemetryLabel_ = makeLabel(4, kL, 588, 684, 20, L"...", false, true);

    makeBtn(4, IDC_EXPORT_BTN, kL, 616, 196, 28, Tr(lang, StrId::BtnExportSettings));
    makeBtn(4, IDC_IMPORT_BTN, kL + 206, 616, 196, 28, Tr(lang, StrId::BtnImportSettings));
    sysStatusLabel_ = makeLabel(4, kL + 412, 620, 272, 22, L"", false, true);

    licenseLabel_ = makeLabel(4, kL, 656, 496, 22, L"", true, true);
    makeBtn(4, IDC_OPEN_PRO_BTN, kL + 506, 652, 178, 30, Tr(lang, StrId::BtnOpenProWindow));

    // ==================== TAB 5: PRO & LİSANS ====================
    proRestoreBtn_ = makeBtn(5, IDC_PRO_RESTORE_BTN, kContentX + 368, 132, 186, 30, Tr(lang, StrId::BtnRestorePurchases));
    proManageBtn_ = makeBtn(5, IDC_PRO_MANAGE_BTN, kContentX + 564, 132, 140, 30, Tr(lang, StrId::BtnManageSubscription));
    proBuyAnnualBtn_ = makeBtn(5, IDC_PRO_BUY_ANNUAL, kContentX + 18, 444, 317, 34, Tr(lang, StrId::BtnBuyAnnual));
    proBuyLifetimeBtn_ = makeBtn(5, IDC_PRO_BUY_LIFETIME, kContentX + 385, 444, 317, 34, Tr(lang, StrId::BtnBuyLifetime));
}

void SettingsWindow::PopulateAllValues() {
    isLoading_ = true;
    AppLanguage lang = settings_.language;

    auto fillCombo = [](HWND combo, const std::vector<std::pair<int, std::wstring>>& items) {
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        for (const auto& [val, label] : items) {
            int idx = static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
            SendMessageW(combo, CB_SETITEMDATA, idx, val);
        }
    };

    // Tab 0
    std::vector<std::pair<int, std::wstring>> styleItems;
    for (int i = 0; i <= 8; ++i) {
        styleItems.emplace_back(i, GetStyleFriendlyNameLocalized(static_cast<TaskbarDisplayStyle>(i), lang));
    }
    fillCombo(styleCombo_, styleItems);
    SetComboSelectionByData(styleCombo_, static_cast<int>(settings_.displayStyle));

    PrayerCountdownState sampleState{};
    sampleState.nextPrayer = PrayerKind::Ikindi;
    sampleState.remainingSeconds = 23 * 60 + 40;
    sampleState.nextPrayerDateTime = {{2026, 11, 25}, 15, 25, 0};
    sampleState.progress = 0.65;
    AppSettings sampleSettings = settings_;
    sampleSettings.showSeconds = false;
    for (int i = 0; i < 6; ++i) {
        sampleSettings.displayStyle = kQuickStyles[i];
        FormattedTaskbarDisplay disp = FormatTaskbar(sampleState, sampleSettings);
        std::wstring label;
        if (kQuickStyles[i] == TaskbarDisplayStyle::IconAndCountdown) {
            label = L"◐ " + disp.line1;
        } else if (kQuickStyles[i] == TaskbarDisplayStyle::MinimalDotTimer) {
            label = L"● " + disp.line1;
        } else if (disp.hasLine2 && !disp.line2.empty()) {
            label = disp.line1 + L" / " + disp.line2;
        } else {
            label = disp.line1;
        }
        if (presetBtns_[i]) SetWindowTextW(presetBtns_[i], label.c_str());
    }

    SetWindowTextW(customEdit_, settings_.customTemplate.c_str());

    std::vector<std::pair<int, std::wstring>> flyoutItems;
    for (int i = 0; i <= 3; ++i) {
        flyoutItems.emplace_back(i, GetFlyoutDesignFriendlyName(static_cast<FlyoutDesignStyle>(i), lang));
    }
    fillCombo(flyoutCombo_, flyoutItems);
    SetComboSelectionByData(flyoutCombo_, static_cast<int>(settings_.flyoutDesign));

    SendMessageW(flyoutScaleSlider_, TBM_SETPOS, TRUE, std::clamp(settings_.flyoutScalePercent, 80, 200));
    wchar_t fsBuf[16];
    swprintf_s(fsBuf, L"%%%d", std::clamp(settings_.flyoutScalePercent, 80, 200));
    SetWindowTextW(flyoutScaleLabel_, fsBuf);

    fillCombo(dockCombo_, {
        {static_cast<int>(TaskbarDockPosition::RightNearTray), Tr(lang, StrId::DockRightTray)},
        {static_cast<int>(TaskbarDockPosition::FarRight),      Tr(lang, StrId::DockFarRight)},
        {static_cast<int>(TaskbarDockPosition::Center),        Tr(lang, StrId::DockCenter)},
        {static_cast<int>(TaskbarDockPosition::LeftSide),      Tr(lang, StrId::DockLeft)}
    });
    SetComboSelectionByData(dockCombo_, static_cast<int>(settings_.dockPosition));

    SendMessageW(offsetSlider_, TBM_SETPOS, TRUE, settings_.horizontalOffsetPx);
    wchar_t offBuf[32];
    swprintf_s(offBuf, L"%d px", settings_.horizontalOffsetPx);
    SetWindowTextW(offsetLabel_, offBuf);

    SetFluentCheck(IDC_SHOW_PILL_CHK, showPillChk_, settings_.showTaskbarPill);
    SetFluentCheck(IDC_OPAQUE_CHK, opaqueChk_, settings_.opaqueTaskbarBackground);
    SetFluentCheck(IDC_SHOW_TRAY_CHK, showTrayChk_, settings_.showTrayIcon);
    SetFluentCheck(IDC_SHOW_BAR_CHK, showBarChk_, settings_.showProgressBar);
    SetFluentCheck(IDC_SHOW_SEC_CHK, showSecChk_, settings_.showSeconds);
    SetFluentCheck(IDC_ACCENT_CHK, accentChk_, settings_.useSystemAccentColor);
    SetFluentCheck(IDC_TASKBAR_FRAME_CHK, taskbarFrameChk_, settings_.showTaskbarFrame);
    SetFluentCheck(IDC_TASKBAR_LOGO_CHK, taskbarLogoChk_, settings_.showTaskbarLogo);

    fillCombo(trayCombo_, {
        {static_cast<int>(TrayIconStyle::TwoLineMini),   Tr(lang, StrId::TrayTwoLine)},
        {static_cast<int>(TrayIconStyle::MinutesRing),   Tr(lang, StrId::TrayRing)},
        {static_cast<int>(TrayIconStyle::CrescentBadge), Tr(lang, StrId::TrayCrescent)}
    });
    int effectiveTrayStyle = (settings_.trayStyle == TrayIconStyle::Hidden) ? 0 : static_cast<int>(settings_.trayStyle);
    SetComboSelectionByData(trayCombo_, effectiveTrayStyle);

    fillCombo(fontCombo_, {
        {static_cast<int>(FontSizePreset::Small),  Tr(lang, StrId::FontSmall)},
        {static_cast<int>(FontSizePreset::Normal), Tr(lang, StrId::FontNormal)},
        {static_cast<int>(FontSizePreset::Large),  Tr(lang, StrId::FontLarge)}
    });
    SetComboSelectionByData(fontCombo_, static_cast<int>(settings_.fontSize));

    std::vector<std::pair<int, std::wstring>> urgentItems;
    for (int i = 0; i < 5; ++i) urgentItems.emplace_back(kUrgentChoices[i], Tr(lang, kUrgentIds[i]));
    fillCombo(urgentCombo_, urgentItems);
    SetComboSelectionByData(urgentCombo_, settings_.highlightUrgentMinutes);

    fillCombo(themeCombo_, {
        {static_cast<int>(AppThemeMode::System), Tr(lang, StrId::ThemeSystem)},
        {static_cast<int>(AppThemeMode::Dark),   Tr(lang, StrId::ThemeDark)},
        {static_cast<int>(AppThemeMode::Light),  Tr(lang, StrId::ThemeLight)}
    });
    SetComboSelectionByData(themeCombo_, static_cast<int>(settings_.themeMode));

    // Tab 1
    SetFluentCheck(IDC_AUTO_DETECT_LOC_CHK, autoDetectLocChk_, settings_.autoDetectLocation);
    SetFluentCheck(IDC_AUTO_METHOD_CHK, autoMethodChk_, settings_.autoSelectMethodByCountry);
    RefreshCityList(L"");
    SetFluentCheck(IDC_CUSTOM_COORD_CHK, customCoordChk_, settings_.useCustomCoordinates);

    wchar_t latBuf[32], lonBuf[32];
    swprintf_s(latBuf, L"%.4f", settings_.latitude);
    swprintf_s(lonBuf, L"%.4f", settings_.longitude);
    SetWindowTextW(latEdit_, latBuf);
    SetWindowTextW(lonEdit_, lonBuf);
    EnableWindow(latEdit_, settings_.useCustomCoordinates);
    EnableWindow(lonEdit_, settings_.useCustomCoordinates);
    EnableWindow(applyCoordBtn_, settings_.useCustomCoordinates);

    std::vector<std::pair<int, std::wstring>> methodItems;
    for (CalculationMethod m : kOrderedCalculationMethods) {
        methodItems.emplace_back(static_cast<int>(m), GetMethodFriendlyName(m));
    }
    fillCombo(methodCombo_, methodItems);
    SetComboSelectionByData(methodCombo_, static_cast<int>(settings_.calculationMethod));

    fillCombo(asrCombo_, {
        {static_cast<int>(AsrJuristicMethod::Standard), Tr(lang, StrId::AsrStandard)},
        {static_cast<int>(AsrJuristicMethod::Hanafi),   Tr(lang, StrId::AsrHanafi)}
    });
    SetComboSelectionByData(asrCombo_, static_cast<int>(settings_.asrMethod));

    SetFluentCheck(IDC_ONLINE_SYNC_CHK, onlineSyncChk_, settings_.useOnlineSync);
    for (int i = 0; i < 6; ++i) {
        int val = settings_.offsets.GetOffset(static_cast<PrayerKind>(i));
        SetWindowTextW(offsetEdits_[i], std::to_wstring(val).c_str());
    }

    // Tab 2 & 3
    std::vector<std::pair<int, std::wstring>> preItems;
    for (int i = 0; i < 8; ++i) preItems.emplace_back(kPreNotifyChoices[i], Tr(lang, kPreNotifyIds[i]));

    for (int i = 0; i < 6; ++i) {
        const auto& rule = settings_.alerts.GetRule(static_cast<PrayerKind>(i));
        SetFluentCheck(static_cast<WORD>(IDC_ALERT_TIME_BASE + i), alertAtTimeChk_[i], rule.notifyAtTime);
        fillCombo(alertPreCombo_[i], preItems);
        SetComboSelectionByData(alertPreCombo_[i], rule.preNotifyMinutes);
        SetFluentCheck(static_cast<WORD>(IDC_ALERT_SOUND_BASE + i), alertSoundChk_[i], rule.playSound);
    }

    std::vector<std::pair<int, std::wstring>> soundItems = {
        {static_cast<int>(NotificationSoundKind::None),        Tr(lang, StrId::SoundNone)},
        {static_cast<int>(NotificationSoundKind::GentleChime), Tr(lang, StrId::SoundChime)},
        {static_cast<int>(NotificationSoundKind::WarmBell),    Tr(lang, StrId::SoundBell)},
        {static_cast<int>(NotificationSoundKind::AdhanShort),  Tr(lang, StrId::SoundAdhanShort)},
        {static_cast<int>(NotificationSoundKind::AdhanMakkah), Tr(lang, StrId::SoundAdhanMakkah)},
        {static_cast<int>(NotificationSoundKind::CustomFile),  Tr(lang, StrId::SoundCustom)}
    };
    fillCombo(entrySoundCombo_, soundItems);
    SetComboSelectionByData(entrySoundCombo_, static_cast<int>(settings_.entrySound));
    fillCombo(preSoundCombo_, soundItems);
    SetComboSelectionByData(preSoundCombo_, static_cast<int>(settings_.preNotifySound));
    std::vector<std::pair<int, std::wstring>> fajrItems = {{-1, Tr(lang, StrId::SoundSameAsEntry)}};
    fajrItems.insert(fajrItems.end(), soundItems.begin(), soundItems.end());
    fillCombo(fajrSoundCombo_, fajrItems);
    SetComboSelectionByData(fajrSoundCombo_, settings_.fajrSound);

    SetWindowTextW(customSoundEdit_, settings_.customSoundPath.c_str());
    SendMessageW(volumeSlider_, TBM_SETPOS, TRUE, settings_.soundVolumePercent);
    wchar_t volBuf[16];
    swprintf_s(volBuf, L"%%%d", settings_.soundVolumePercent);
    SetWindowTextW(volumeLabel_, volBuf);

    SetFluentCheck(IDC_QUIET_FULLSCREEN_CHK, quietFullscreenChk_, settings_.quietDuringFullscreen);
    SetFluentCheck(IDC_KERAHAT_CHK, kerahatChk_, settings_.showKerahatIndicator);
    SetFluentCheck(IDC_RAMADAN_CHK, ramadanChk_, settings_.ramadanIftarSahurMode);
    SetFluentCheck(IDC_FRIDAY_CHK, fridayChk_, settings_.fridayReminder);
    SetFluentCheck(IDC_HOLIDAY_CHK, holidayChk_, settings_.holidayReminders);
    SetFluentCheck(IDC_EXTRA_TIMES_CHK, extraTimesChk_, settings_.showExtraTimes);
    SetFluentCheck(IDC_TRAVEL_CHK, travelChk_, settings_.travelMode);
    SetFluentCheck(IDC_TRACKING_CHK, trackingChk_, settings_.prayerTracking);
    SetFluentCheck(IDC_TOAST_CHK, toastChk_, settings_.modernToasts);
    SetFluentCheck(IDC_HOTKEY_CHK, hotkeyChk_, settings_.globalHotkey);
    UpdateHotkeyUi();

    // Tab 4
    std::vector<std::pair<int, std::wstring>> langItems;
    for (int i = 0; i <= 12; ++i) {
        langItems.emplace_back(i, GetLanguageDisplayName(static_cast<AppLanguage>(i)));
    }
    fillCombo(langCombo_, langItems);
    SetComboSelectionByData(langCombo_, static_cast<int>(settings_.language));

    for (int i = 0; i < 6; ++i) {
        SetWindowTextW(customNameEdits_[i], settings_.customPrayerNames[static_cast<size_t>(i)].c_str());
        SetWindowTextW(customShortEdits_[i], settings_.customPrayerShortCodes[static_cast<size_t>(i)].c_str());
    }

    SetFluentCheck(IDC_STARTUP_CHK, startupChk_, IsStartupEnabled());
    SetFluentCheck(IDC_AUTO_MEM_CHK, autoMemChk_, settings_.autoOptimizeMemory);

    isLoading_ = false;
    UpdateTab0CustomTemplateVisibility();
    RefreshPreviewAndMemory();
    RefreshLicenseStatus();
}

void SettingsWindow::UpdateTab0CustomTemplateVisibility() {
    if (activeTab_ != 0) return;
    bool isCustom = (settings_.displayStyle == TaskbarDisplayStyle::Custom);
    for (HWND h : presetBtns_) {
        if (h) {
            ShowWindow(h, isCustom ? SW_HIDE : SW_SHOW);
            if (!isCustom) InvalidateRect(h, nullptr, FALSE);
        }
    }
    if (customTemplateLabel_) ShowWindow(customTemplateLabel_, isCustom ? SW_SHOW : SW_HIDE);
    if (customEdit_) ShowWindow(customEdit_, isCustom ? SW_SHOW : SW_HIDE);
    for (HWND h : tokenBtns_) {
        if (h) ShowWindow(h, isCustom ? SW_SHOW : SW_HIDE);
    }
}

void SettingsWindow::RefreshCityList(const std::wstring& query) {
    visibleCities_ = SearchCities(query, 80);
    SendMessageW(cityList_, LB_RESETCONTENT, 0, 0);
    int selIdx = -1;
    for (size_t i = 0; i < visibleCities_.size(); ++i) {
        std::wstring lbl = visibleCities_[i].DisplayLabel();
        SendMessageW(cityList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(lbl.c_str()));
        if (visibleCities_[i].id == settings_.cityId) {
            selIdx = static_cast<int>(i);
        }
    }
    if (selIdx >= 0) {
        SendMessageW(cityList_, LB_SETCURSEL, selIdx, 0);
    }
}

void SettingsWindow::StartWorldwideCitySearch(const std::wstring& query) {
    HWND target = hwnd_;
    std::thread([target, query]() {
        auto* result = new CitySearchResults{query, ScheduleService::SearchWorldwideCities(query)};
        if (!PostMessageW(target, kMsgCitySearchResults, 0, reinterpret_cast<LPARAM>(result))) {
            delete result;
        }
    }).detach();
}

void SettingsWindow::ApplyDetectedLocation(const DetectedLocationResult& location) {
    settings_.cityId = location.cityId;
    settings_.cityName = location.cityName;
    settings_.countryName = location.countryName;
    settings_.latitude = location.latitude;
    settings_.longitude = location.longitude;
    settings_.timeZoneId = location.timeZoneId;
    settings_.useCustomCoordinates = false;
    if (settings_.autoSelectMethodByCountry) {
        settings_.calculationMethod = location.recommendedMethod;
    }

    if (!hwnd_) return;
    isLoading_ = true;
    SetFluentCheck(IDC_AUTO_DETECT_LOC_CHK, autoDetectLocChk_, settings_.autoDetectLocation);
    SetFluentCheck(IDC_CUSTOM_COORD_CHK, customCoordChk_, false);
    EnableWindow(latEdit_, FALSE);
    EnableWindow(lonEdit_, FALSE);
    EnableWindow(applyCoordBtn_, FALSE);
    if (settings_.autoSelectMethodByCountry) {
        SetComboSelectionByData(methodCombo_, static_cast<int>(settings_.calculationMethod));
    }

    wchar_t latBuf[32], lonBuf[32];
    swprintf_s(latBuf, L"%.4f", settings_.latitude);
    swprintf_s(lonBuf, L"%.4f", settings_.longitude);
    SetWindowTextW(latEdit_, latBuf);
    SetWindowTextW(lonEdit_, lonBuf);
    RefreshCityList(GetEditText(citySearchEdit_));
    std::wstring statusMsg = location.cityName + L", " + location.countryName + L" (" + location.sourceLabel + L") · " +
                             GetMethodFriendlyName(settings_.calculationMethod);
    SetWindowTextW(autoLocStatus_, statusMsg.c_str());
    isLoading_ = false;
    RefreshPreviewAndMemory();
}

void SettingsWindow::SwitchTab(int tabIdx) {
    activeTab_ = std::clamp(tabIdx, 0, 5);
    if (activeTab_ != 5) proLockedHint_ = false;
    for (int t = 0; t < 6; ++t) {
        int showCmd = (t == activeTab_) ? SW_SHOW : SW_HIDE;
        for (HWND h : tabControls_[t]) {
            ShowWindow(h, showCmd);
        }
        InvalidateRect(tabBtns_[t], nullptr, FALSE);
        AnnotateAccessible(tabBtns_[t], ROLE_SYSTEM_PAGETAB,
                           STATE_SYSTEM_FOCUSABLE | (t == activeTab_ ? STATE_SYSTEM_SELECTED : 0));
    }
    UpdateTab0CustomTemplateVisibility();
    InvalidateRect(hwnd_, nullptr, FALSE);
    if (activeTab_ >= 4) {
        RefreshPreviewAndMemory();
    }
}

void SettingsWindow::SaveAndNotify() {
    if (isLoading_) return;
    settingsStore_.Save(settings_);
    if (onSettingsSaved_) onSettingsSaved_();
    RefreshPreviewAndMemory();
    UpdateHotkeyUi();
}

namespace {
std::wstring FormatHotkey(int mods, int vk) {
    std::wstring out;
    if (mods & MOD_WIN) out += L"Win+";
    if (mods & MOD_CONTROL) out += L"Ctrl+";
    if (mods & MOD_ALT) out += L"Alt+";
    if (mods & MOD_SHIFT) out += L"Shift+";
    if (vk >= VK_F1 && vk <= VK_F24) {
        out += L"F" + std::to_wstring(vk - VK_F1 + 1);
    } else if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        out += static_cast<wchar_t>(vk);
    } else {
        UINT sc = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
        bool ext = (vk >= VK_PRIOR && vk <= VK_DELETE) || vk == VK_DIVIDE || vk == VK_NUMLOCK;
        wchar_t name[64]{};
        if (GetKeyNameTextW(static_cast<LONG>((sc << 16) | (ext ? (1u << 24) : 0u)), name, 64) > 0) out += name;
        else out += L"#" + std::to_wstring(vk);
    }
    return out;
}
} // namespace

void SettingsWindow::UpdateHotkeyUi() {
    if (!hotkeyBtn_ || capturingHotkey_) return;
    AppLanguage lang = settings_.language;
    SetWindowTextW(hotkeyBtn_, FormatHotkey(settings_.hotkeyMods, settings_.hotkeyVk).c_str());
    bool taken = settings_.globalHotkey && isHotkeyOk_ && !isHotkeyOk_();
    if (hotkeyDesc_) SetWindowTextW(hotkeyDesc_, Tr(lang, taken ? StrId::HotkeyInUse : StrId::DescHotkey));
    InvalidateRect(hotkeyBtn_, nullptr, FALSE);
}

void SettingsWindow::BeginHotkeyCapture() {
    capturingHotkey_ = true;
    SetWindowTextW(hotkeyBtn_, Tr(settings_.language, StrId::HotkeyPressKeys));
    InvalidateRect(hotkeyBtn_, nullptr, FALSE);
    SetFocus(hotkeyBtn_);
}

LRESULT CALLBACK SettingsWindow::HotkeyBtnProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref) {
    auto* self = reinterpret_cast<SettingsWindow*>(ref);
    if (!self || !self->capturingHotkey_) return DefSubclassProc(h, msg, wp, lp);
    switch (msg) {
        case WM_GETDLGCODE:
            return DLGC_WANTALLKEYS;
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_KEYUP:
        case WM_SYSKEYUP:
            return 0;
        case WM_KILLFOCUS:
            self->capturingHotkey_ = false;
            self->UpdateHotkeyUi();
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            int vk = static_cast<int>(wp);
            if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN ||
                vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LCONTROL || vk == VK_RCONTROL ||
                vk == VK_LMENU || vk == VK_RMENU) {
                return 0;
            }
            int mods = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
            if (GetKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
            if (GetKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
            if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= MOD_WIN;
            if (vk == VK_ESCAPE && mods == 0) {
                self->capturingHotkey_ = false;
                self->UpdateHotkeyUi();
                return 0;
            }
            if (mods == 0) {
                if (self->hotkeyDesc_) SetWindowTextW(self->hotkeyDesc_, Tr(self->settings_.language, StrId::HotkeyNeedsModifier));
                return 0;
            }
            self->capturingHotkey_ = false;
            self->settings_.hotkeyMods = mods;
            self->settings_.hotkeyVk = vk;
            self->settings_.globalHotkey = true;
            self->SetFluentCheck(IDC_HOTKEY_CHK, self->hotkeyChk_, true);
            self->SaveAndNotify();
            return 0;
        }
    }
    return DefSubclassProc(h, msg, wp, lp);
}

void SettingsWindow::SetProBusy(bool busy) {
    proBusy_ = busy;
    BOOL en = busy ? FALSE : TRUE;
    if (proBuyAnnualBtn_) EnableWindow(proBuyAnnualBtn_, en);
    if (proBuyLifetimeBtn_) EnableWindow(proBuyLifetimeBtn_, en);
    if (proRestoreBtn_) EnableWindow(proRestoreBtn_, en);
}

void SettingsWindow::StartProStorePurchase(const wchar_t* addOnStoreId) {
    if (proBusy_ || !hwnd_) return;
    SetProBusy(true);
    proFeedbackIsError_ = false;
    proFeedbackText_ = Tr(settings_.language, StrId::StorePurchasing);
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

void SettingsWindow::StartProStoreRestore() {
    if (!hwnd_) return;
    SetProBusy(true);
    proFeedbackIsError_ = false;
    proFeedbackText_ = Tr(settings_.language, StrId::StoreRestoring);
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

void SettingsWindow::PaintProTabCanvas(void* g, float s) {
    auto pal = theme::GetUiPalette(isDark_, settings_.useSystemAccentColor);
    AppLanguage lang = settings_.language;
    LicenseStatus lst = licenseManager_.GetStatus();

    void* uiFam = gdip::CreateBestUiFontFamily(false);
    void* dispFam = gdip::CreateBestUiFontFamily(true);

    // 1. Üst durum kartı (y=118, h=58)
    float stX = static_cast<float>(kContentX) * s;
    float stY = 118.0f * s;
    float stW = static_cast<float>(kContentW) * s;
    float stH = 58.0f * s;
    gdip::FillRoundedRect(g, stX, stY, stW, stH, 10.0f * s, pal.bgCard);
    gdip::StrokeRoundedRect(g, stX, stY, stW, stH, 10.0f * s, pal.border, 1.1f);

    std::wstring statusText;
    uint32_t statusColor = pal.accent;
    if (proLockedHint_) {
        statusText = Tr(lang, StrId::ProLockedHint);
        statusColor = pal.gold;
    } else if (lst.tier == LicenseTier::ProLifetime) {
        statusText = Tr(lang, StrId::ProStatusActiveLifetime);
        statusColor = pal.success;
    } else if (lst.tier == LicenseTier::ProAnnual) {
        statusText = Tr(lang, StrId::ProStatusActiveAnnual);
        statusColor = pal.success;
    } else if (lst.tier == LicenseTier::Trial) {
        wchar_t buf[128];
        swprintf_s(buf, L"%ls (%d)", Tr(lang, StrId::ProStatusTrial), lst.trialDaysRemaining);
        statusText = buf;
        statusColor = pal.accent;
    } else {
        statusText = Tr(lang, StrId::ProStatusFree);
        statusColor = pal.gold;
    }
    gdip::DrawTextBox(g, statusText, dispFam, 14.0f * s, true, statusColor,
                      stX + 18.0f * s, stY + 4.0f * s, 336.0f * s, stH - 8.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

    // 2. İki plan kartı (y=188, h=304)
    float cardY = 188.0f * s;
    float cardH = 304.0f * s;
    float cardW = 353.0f * s;
    float leftX = static_cast<float>(kContentX) * s;
    float rightX = static_cast<float>(kContentX + 367) * s;

    // Sol kart: Yıllık Pro
    gdip::FillRoundedRect(g, leftX, cardY, cardW, cardH, 10.0f * s, pal.bgCard);
    gdip::StrokeRoundedRect(g, leftX, cardY, cardW, cardH, 10.0f * s, pal.border, 1.2f);

    float badgeW = cardW - 56.0f * s;
    gdip::FillRoundedRect(g, leftX + 28.0f * s, cardY + 14.0f * s, badgeW, 22.0f * s, 11.0f * s, pal.chipBg);
    gdip::StrokeRoundedRect(g, leftX + 28.0f * s, cardY + 14.0f * s, badgeW, 22.0f * s, 11.0f * s, pal.border, 1.0f);
    gdip::DrawTextBox(g, Tr(lang, StrId::ProBadgeFlexible), uiFam, 10.2f * s, true, pal.textSecondary,
                      leftX + 28.0f * s, cardY + 14.0f * s, badgeW, 22.0f * s,
                      gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);

    gdip::DrawTextBox(g, Tr(lang, StrId::ProPlanAnnualTitle), dispFam, 15.5f * s, true, pal.textPrimary,
                      leftX + 18.0f * s, cardY + 46.0f * s, cardW - 36.0f * s, 24.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
    gdip::DrawTextBox(g, Tr(lang, StrId::ProPlanAnnualPrice), dispFam, 24.0f * s, true, pal.textPrimary,
                      leftX + 18.0f * s, cardY + 72.0f * s, cardW - 36.0f * s, 32.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
    gdip::DrawTextBox(g, Tr(lang, StrId::ProAnnualSub), uiFam, 12.0f * s, false, pal.textSecondary,
                      leftX + 18.0f * s, cardY + 108.0f * s, cardW - 36.0f * s, 20.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

    const StrId annualBullets[3] = {StrId::ProBulletAllFeatures, StrId::ProBulletSubUpdates, StrId::ProBulletCancelAnytime};
    for (int i = 0; i < 3; ++i) {
        float by = cardY + 142.0f * s + i * 30.0f * s;
        gdip::DrawTextBox(g, L"✓", uiFam, 12.8f * s, true, pal.accent,
                          leftX + 18.0f * s, by, 20.0f * s, 22.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, Tr(lang, annualBullets[i]), uiFam, 12.8f * s, false, pal.textPrimary,
                          leftX + 40.0f * s, by, cardW - 58.0f * s, 22.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
    }

    // Sağ kart: Ömür Boyu Pro (Zümrüt Vurgulu)
    gdip::FillRoundedRect(g, rightX, cardY, cardW, cardH, 10.0f * s, pal.bgHighlight);
    gdip::StrokeRoundedRect(g, rightX, cardY, cardW, cardH, 10.0f * s, pal.borderAccent, 1.8f);

    gdip::FillRoundedRect(g, rightX + 28.0f * s, cardY + 14.0f * s, badgeW, 22.0f * s, 11.0f * s, theme::WithAlpha(0x36u, pal.accent));
    gdip::StrokeRoundedRect(g, rightX + 28.0f * s, cardY + 14.0f * s, badgeW, 22.0f * s, 11.0f * s, pal.borderAccentSoft, 1.0f);
    gdip::DrawTextBox(g, Tr(lang, StrId::ProBadgePopular), uiFam, 10.2f * s, true, pal.accent,
                      rightX + 28.0f * s, cardY + 14.0f * s, badgeW, 22.0f * s,
                      gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);

    gdip::DrawTextBox(g, Tr(lang, StrId::ProPlanLifetimeTitle), dispFam, 15.5f * s, true, pal.textPrimary,
                      rightX + 18.0f * s, cardY + 46.0f * s, cardW - 36.0f * s, 24.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
    gdip::DrawTextBox(g, Tr(lang, StrId::ProPlanLifetimePrice), dispFam, 24.0f * s, true, pal.gold,
                      rightX + 18.0f * s, cardY + 72.0f * s, cardW - 36.0f * s, 32.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
    gdip::DrawTextBox(g, Tr(lang, StrId::ProLifetimeSub), uiFam, 12.0f * s, false, pal.textSecondary,
                      rightX + 18.0f * s, cardY + 108.0f * s, cardW - 36.0f * s, 20.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

    const StrId lifeBullets[4] = {
        StrId::ProBulletAllFeatures, StrId::ProBulletAllUpdates,
        StrId::ProBulletOneTime, StrId::ProBulletBestValue
    };
    for (int i = 0; i < 4; ++i) {
        float by = cardY + 138.0f * s + i * 26.0f * s;
        bool isStar = (i == 3);
        gdip::DrawTextBox(g, isStar ? L"★" : L"✓", uiFam, 12.8f * s, true, isStar ? pal.gold : pal.accent,
                          rightX + 18.0f * s, by, 20.0f * s, 22.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, Tr(lang, lifeBullets[i]), uiFam, 12.8f * s, isStar, isStar ? pal.gold : pal.textPrimary,
                          rightX + 40.0f * s, by, cardW - 58.0f * s, 22.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
    }

    // 3. Alt özellikler kartı (y=504, h=190)
    float featY = 504.0f * s;
    float featH = 190.0f * s;
    gdip::FillRoundedRect(g, leftX, featY, stW, featH, 10.0f * s, pal.bgCard);
    gdip::StrokeRoundedRect(g, leftX, featY, stW, featH, 10.0f * s, pal.border, 1.1f);

    gdip::DrawTextBox(g, Tr(lang, StrId::ProIncludedBoth), dispFam, 13.8f * s, true, pal.textPrimary,
                      leftX + 18.0f * s, featY + 12.0f * s, stW - 36.0f * s, 22.0f * s,
                      gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

    const StrId feats[5] = {StrId::ProFeat1, StrId::ProFeat2, StrId::ProFeat3, StrId::ProFeat4, StrId::ProFeat5};
    for (int i = 0; i < 5; ++i) {
        float fy = featY + 42.0f * s + i * 27.0f * s;
        gdip::DrawTextBox(g, L"✓", uiFam, 12.6f * s, true, pal.accent,
                          leftX + 18.0f * s, fy, 20.0f * s, 22.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
        gdip::DrawTextBox(g, StripLeadingBullet(Tr(lang, feats[i])), uiFam, 12.6f * s, false, pal.textPrimary,
                          leftX + 40.0f * s, fy, stW - 58.0f * s, 22.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
    }

    if (!proFeedbackText_.empty()) {
        uint32_t fbCol = proFeedbackIsError_ ? pal.danger : pal.success;
        gdip::DrawTextBox(g, proFeedbackText_, uiFam, 12.2f * s, true, fbCol,
                          leftX, 704.0f * s, stW, 24.0f * s,
                          gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
    }

    if (dispFam) gdip::GdipDeleteFontFamily(dispFam);
    if (uiFam) gdip::GdipDeleteFontFamily(uiFam);
}

void SettingsWindow::PaintBackgroundAndCards(HDC hdc, int width, int height) {
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

            auto pal = theme::GetUiPalette(isDark_, settings_.useSystemAccentColor);
            float s = static_cast<float>(scale_);

            gdip::GdipGraphicsClear(g, pal.bgWindow);

            // Sol kenar çubuğu arka planı ve ince dikey ayracı
            gdip::FillRoundedRect(g, 0.0f, 0.0f, static_cast<float>(kSidebarW) * s, static_cast<float>(height), 0.0f, pal.bgSidebar);
            void* divPen = nullptr;
            if (gdip::GdipCreatePen1(pal.border, std::max(1.0f, 1.0f * s), gdip::kUnitPixel, &divPen) == 0 && divPen) {
                gdip::GdipDrawLine(g, divPen, static_cast<float>(kSidebarW) * s, 0.0f, static_cast<float>(kSidebarW) * s, static_cast<float>(height));
                gdip::GdipDeletePen(divPen);
            }

            // Sol üst marka logosu + adı
            void* dispFam = gdip::CreateBestUiFontFamily(true);
            theme::DrawBrandLogoTile(g, 16.0f * s, 14.0f * s, 32.0f * s);
            gdip::DrawTextBox(g, L"PrayerBar", dispFam, 18.5f * s, true, pal.textPrimary,
                              56.0f * s, 14.0f * s, 148.0f * s, 32.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            // Sekme 4 ile Sekme 5 (Pro & Lisans) arasında yatay ayırıcı + alt çizgi
            void* subPen = nullptr;
            if (gdip::GdipCreatePen1(pal.borderSubtle, std::max(1.0f, 1.0f * s), gdip::kUnitPixel, &subPen) == 0 && subPen) {
                gdip::GdipDrawLine(g, subPen, 16.0f * s, 303.0f * s, (static_cast<float>(kSidebarW) - 16.0f) * s, 303.0f * s);
                gdip::GdipDrawLine(g, subPen, static_cast<float>(kContentX) * s, 754.0f * s,
                                   static_cast<float>(kContentX + kContentW) * s, 754.0f * s);
                gdip::GdipDeletePen(subPen);
            }

            // Aktif sayfa başlığı
            const StrId titleIds[6] = {
                StrId::TabAppearance, StrId::TabLocation, StrId::TabNotifications,
                StrId::TabFeatures, StrId::TabLanguageSystem, StrId::TabProLicense
            };
            const wchar_t* pageTitle = Tr(settings_.language, titleIds[std::clamp(activeTab_, 0, 5)]);
            gdip::DrawTextBox(g, pageTitle, dispFam, 21.5f * s, true, pal.textPrimary,
                              static_cast<float>(kContentX) * s, 80.0f * s, static_cast<float>(kContentW) * s, 32.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            if (activeTab_ == 5) {
                PaintProTabCanvas(g, s);
            } else {
                for (const auto& box : GetCardsForTab(activeTab_)) {
                    gdip::FillRoundedRect(g, box.x * s, box.y * s, box.w * s, box.h * s, 10.0f * s, pal.bgCard);
                    gdip::StrokeRoundedRect(g, box.x * s, box.y * s, box.w * s, box.h * s, 10.0f * s, pal.border, 1.1f);
                }
                if (activeTab_ == 3) {
                    void* rowPen = nullptr;
                    if (gdip::GdipCreatePen1(pal.borderSubtle, 1.0f, gdip::kUnitPixel, &rowPen) == 0 && rowPen) {
                        const float rowSepY[] = {188.0f, 238.0f, 288.0f, 338.0f, 474.0f, 510.0f, 560.0f, 610.0f};
                        for (float ry : rowSepY) {
                            gdip::GdipDrawLine(g, rowPen, (kContentX + 18.0f) * s, ry * s, (kContentX + kContentW - 18.0f) * s, ry * s);
                        }
                        gdip::GdipDeletePen(rowPen);
                    }
                }
            }

            if (dispFam) gdip::GdipDeleteFontFamily(dispFam);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gdiBmp);
    }

    BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDc);
}

void SettingsWindow::PaintSidebarTab(DRAWITEMSTRUCT* dis, int tabIdx) {
    gdip::EnsureInitialized();
    int w = dis->rcItem.right - dis->rcItem.left;
    int h = dis->rcItem.bottom - dis->rcItem.top;
    if (w <= 0 || h <= 0) return;

    auto pal = theme::GetUiPalette(isDark_, settings_.useSystemAccentColor);
    bool active = (tabIdx == activeTab_);
    bool hovered = (dis->hwndItem == hoveredBtn_);
    bool isProTab = (tabIdx == 5);

    uint32_t rowBg = active ? pal.bgNavActive : (hovered ? pal.bgNavHover : pal.bgSidebar);
    uint32_t fg = active ? pal.textPrimary : (hovered ? pal.textPrimary : pal.textSecondary);
    uint32_t iconCol = isProTab
        ? pal.gold
        : (active ? pal.accent : (hovered ? pal.textPrimary : pal.textSecondary));

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

    HGDIOBJ oldBmp = SelectObject(memDc, bmp);
    void* gBmp = nullptr;
    void* g = nullptr;
    if (gdip::GdipCreateBitmapFromScan0(w, h, w * 4, gdip::kPixelFormat32bppPARGB, bits, &gBmp) == 0 && gBmp) {
        if (gdip::GdipGetImageGraphicsContext(gBmp, &g) == 0 && g) {
            gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAlias);
            gdip::GdipGraphicsClear(g, pal.bgSidebar);

            float s = static_cast<float>(scale_);
            float fw = static_cast<float>(w);
            float fh = static_cast<float>(h);

            if (active || hovered) {
                gdip::FillRoundedRect(g, 1.0f, 1.0f, fw - 2.0f, fh - 2.0f, 8.0f * s, rowBg);
                if (active) {
                    uint32_t bdr = isProTab ? theme::WithAlpha(0x88u, pal.gold) : pal.borderAccentSoft;
                    gdip::StrokeRoundedRect(g, 1.0f, 1.0f, fw - 2.0f, fh - 2.0f, 8.0f * s, bdr, 1.0f);
                    gdip::FillRoundedRect(g, 4.0f * s, 9.0f * s, 3.5f * s, fh - 18.0f * s, 2.0f * s, isProTab ? pal.gold : pal.accent);
                }
            }

            static constexpr const wchar_t* kGlyphs[6] = {L"\uE790", L"\uE707", L"\uEA8F", L"\uE708", L"\uE774", L"\uE734"};
            static constexpr const wchar_t* kFallbacks[6] = {L"◐", L"◎", L"♪", L"✦", L"⚙", L"★"};
            static constexpr StrId kTitleIds[6] = {
                StrId::TabAppearance, StrId::TabLocation, StrId::TabNotifications,
                StrId::TabFeatures, StrId::TabLanguageSystem, StrId::TabProLicense
            };

            void* iconFam = gdip::CreateFluentIconFontFamily();
            void* uiFam = gdip::CreateBestUiFontFamily(false);
            int idx = std::clamp(tabIdx, 0, 5);
            gdip::DrawFluentIcon(g, iconFam, kGlyphs[idx], uiFam, kFallbacks[idx], 14.5f * s, iconCol,
                                 12.0f * s, 0.0f, 24.0f * s, fh);
            gdip::DrawTextBox(g, Tr(settings_.language, kTitleIds[idx]), uiFam, 13.8f * s, active || isProTab, fg,
                              42.0f * s, 0.0f, fw - 48.0f * s, fh,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            if (iconFam) gdip::GdipDeleteFontFamily(iconFam);
            if (uiFam) gdip::GdipDeleteFontFamily(uiFam);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gBmp);
    }

    BitBlt(dis->hDC, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
}

void SettingsWindow::PaintOwnerPushButton(DRAWITEMSTRUCT* dis) {
    gdip::EnsureInitialized();
    int w = dis->rcItem.right - dis->rcItem.left;
    int h = dis->rcItem.bottom - dis->rcItem.top;
    if (w <= 0 || h <= 0) return;

    WORD id = static_cast<WORD>(dis->CtlID);
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    bool hovered = (dis->hwndItem == hoveredBtn_) && !disabled;
    bool accentBtn = IsAccentButton(id);
    bool inCard = IsControlInCard(id);

    // Highlight active Flyout size preset or Taskbar style preset button
    bool presetActive =
        (id == IDC_FLYOUT_SIZE_SM && settings_.flyoutScalePercent <= 105) ||
        (id == IDC_FLYOUT_SIZE_MD && settings_.flyoutScalePercent > 105 && settings_.flyoutScalePercent <= 132) ||
        (id == IDC_FLYOUT_SIZE_LG && settings_.flyoutScalePercent > 132);
    if (id >= IDC_PRESET_BASE && id < IDC_PRESET_BASE + 6 &&
        settings_.displayStyle == kQuickStyles[id - IDC_PRESET_BASE]) {
        presetActive = true;
    }

    auto pal = theme::GetUiPalette(isDark_, settings_.useSystemAccentColor);
    uint32_t parentBg = (id == IDC_PRO_BUY_LIFETIME)
        ? pal.bgHighlight
        : (inCard ? pal.bgCard : pal.bgWindow);

    uint32_t fill = 0;
    uint32_t border = 0;
    uint32_t fg = 0;
    if (accentBtn) {
        fill = disabled
            ? pal.bgBtnPressed
            : (pressed ? theme::WithAlpha(0xCCu, pal.accent) : (hovered ? theme::WithAlpha(0xE8u, pal.accent) : pal.accent));
        border = disabled ? pal.border : pal.accent;
        fg = disabled ? pal.textMuted : pal.accentTextOnFill;
    } else if (presetActive) {
        fill = pal.bgHighlight;
        border = pal.borderAccent;
        fg = pal.accent;
    } else {
        fill = pressed
            ? pal.bgBtnPressed
            : (hovered ? pal.bgBtnHover : pal.bgBtn);
        border = hovered ? pal.borderAccentSoft : pal.border;
        fg = disabled ? pal.textMuted : pal.textPrimary;
    }

    std::wstring label = GetEditText(dis->hwndItem);

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

    HGDIOBJ oldBmp = SelectObject(memDc, bmp);
    void* gBmp = nullptr;
    void* g = nullptr;
    if (gdip::GdipCreateBitmapFromScan0(w, h, w * 4, gdip::kPixelFormat32bppPARGB, bits, &gBmp) == 0 && gBmp) {
        if (gdip::GdipGetImageGraphicsContext(gBmp, &g) == 0 && g) {
            gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAlias);
            gdip::GdipGraphicsClear(g, parentBg);

            float s = static_cast<float>(scale_);
            gdip::FillRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 6.0f * s, fill);
            gdip::StrokeRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 6.0f * s, border, presetActive ? 1.3f : 1.0f);

            void* fam = gdip::CreateBestUiFontFamily(false);
            gdip::DrawTextBox(g, label, fam, 13.0f * s, accentBtn || presetActive, fg,
                              4.0f * s, 0.0f, w - 8.0f * s, static_cast<float>(h),
                              gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
            if (fam) gdip::GdipDeleteFontFamily(fam);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gBmp);
    }

    BitBlt(dis->hDC, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
}

void SettingsWindow::PaintOwnerCheckbox(DRAWITEMSTRUCT* dis) {
    gdip::EnsureInitialized();
    int w = dis->rcItem.right - dis->rcItem.left;
    int h = dis->rcItem.bottom - dis->rcItem.top;
    if (w <= 0 || h <= 0) return;

    WORD id = static_cast<WORD>(dis->CtlID);
    bool checked = GetFluentCheck(id);
    bool inCard = IsControlInCard(id);
    bool rightToggle = rightToggleIds_.count(id) > 0;
    auto pal = theme::GetUiPalette(isDark_, settings_.useSystemAccentColor);
    uint32_t parentBg = inCard ? pal.bgCard : pal.bgWindow;

    std::wstring label = GetEditText(dis->hwndItem);

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

    HGDIOBJ oldBmp = SelectObject(memDc, bmp);
    void* gBmp = nullptr;
    void* g = nullptr;
    if (gdip::GdipCreateBitmapFromScan0(w, h, w * 4, gdip::kPixelFormat32bppPARGB, bits, &gBmp) == 0 && gBmp) {
        if (gdip::GdipGetImageGraphicsContext(gBmp, &g) == 0 && g) {
            gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAlias);
            gdip::GdipGraphicsClear(g, parentBg);

            float s = static_cast<float>(scale_);
            void* fam = gdip::CreateBestUiFontFamily(false);

            if (rightToggle) {
                float tw = 42.0f * s;
                float th = 22.0f * s;
                float tx = static_cast<float>(w) - tw - 2.0f * s;
                float ty = (static_cast<float>(h) - th) * 0.5f;
                theme::DrawFluentToggleSwitch(g, tx, ty, tw, th, checked, pal);

                float textMaxW = (id == IDC_HOTKEY_CHK) ? (tx - 168.0f * s) : (tx - 12.0f * s);
                gdip::DrawTextBox(g, label, fam, 13.8f * s, true, pal.textPrimary,
                                  4.0f * s, 0.0f, std::max(40.0f, textMaxW), static_cast<float>(h),
                                  gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            } else {
                float tw = 34.0f * s;
                float th = 18.0f * s;
                float ty = (static_cast<float>(h) - th) * 0.5f;
                theme::DrawFluentToggleSwitch(g, 1.0f * s, ty, tw, th, checked, pal);

                gdip::DrawTextBox(g, label, fam, 13.2f * s, false, pal.textPrimary,
                                  tw + 10.0f * s, 0.0f, static_cast<float>(w) - tw - 12.0f * s, static_cast<float>(h),
                                  gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            }

            if (fam) gdip::GdipDeleteFontFamily(fam);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gBmp);
    }

    BitBlt(dis->hDC, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
}

void SettingsWindow::PaintTaskbarPreview(HDC hdc, int width, int height) {
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

            auto pal = theme::GetUiPalette(isDark_, settings_.useSystemAccentColor);
            float s = static_cast<float>(scale_);
            float w = static_cast<float>(width);
            float h = static_cast<float>(height);
            bool tbLight = IsTaskbarLight(settings_.themeMode);
            bool useOpaque = settings_.opaqueTaskbarBackground || settings_.dockPosition == TaskbarDockPosition::FarRight;

            gdip::GdipGraphicsClear(g, pal.bgWindow);
            uint32_t tbBg = tbLight ? 0xFFE5EEF0u : 0xFF0A1016u;
            gdip::FillRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 10.0f * s, tbBg);
            gdip::StrokeRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 10.0f * s, pal.border, 1.1f);

            LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
            PrayerCountdownState state = scheduleService_.ComputeState(now, settings_);
            FormattedTaskbarDisplay disp = FormatTaskbar(state, settings_);

            void* family = gdip::CreateBestUiFontFamily(false);
            uint32_t mockTxt = tbLight ? 0xFF425666u : 0xFF9EB3C4u;
            auto tbPal = theme::GetUiPalette(!tbLight, settings_.useSystemAccentColor);

            uint32_t pillBg = 0;
            uint32_t pillBdr = 0;
            bool drawPillBdr = true;
            uint32_t pillPrim = 0;
            uint32_t pillSec = 0;
            uint32_t singleTxt = 0;
            uint32_t accent = disp.isUrgent ? tbPal.gold : tbPal.accent;

            if (!settings_.showTaskbarFrame) {
                drawPillBdr = false;
                pillBg = disp.isUrgent ? (tbLight ? 0xFFFEF3C7u : 0xFF3B260Cu)
                                       : (useOpaque ? (tbLight ? 0xFFFFFFFFu : 0xFF14181Fu) : 0x00000000u);
                pillPrim = disp.isUrgent ? (tbLight ? 0xFFB45309u : tbPal.gold) : (tbLight ? 0xFF0F1B22u : 0xFFFFFFFFu);
                pillSec = tbLight ? 0xFF334155u : 0xFFCBD5E1u;
                singleTxt = pillPrim;
            } else if (settings_.useSystemAccentColor || settings_.pillColorMode == TaskbarPillColorMode::ThemeAdaptive) {
                pillBg = disp.isUrgent ? (tbLight ? 0xFFFEF3C7u : 0xFF3B260Cu) : (tbLight ? 0xFFFFFFFFu : 0xFF161B22u);
                pillBdr = disp.isUrgent ? tbPal.gold : theme::WithAlpha(0xBBu, tbPal.accent);
                pillPrim = disp.isUrgent ? (tbLight ? 0xFFB45309u : tbPal.gold) : (tbLight ? 0xFF0F172Au : 0xFFF8FAFCu);
                pillSec = tbLight ? 0xFF475569u : 0xFFCBD5E1u;
                singleTxt = pillPrim;
            } else {
                pillBg = tbLight
                    ? (disp.isUrgent ? 0xFFFEF3C7u : (useOpaque ? 0xFFFFFFFFu : 0xEBFFFFFFu))
                    : (disp.isUrgent ? 0xFF3B260Cu : (useOpaque ? 0xFF0F3634u : 0xDC0E2E2Du));
                pillBdr = disp.isUrgent ? tbPal.gold : tbPal.borderAccentSoft;
                pillPrim = tbLight ? (disp.isUrgent ? 0xFFB45309u : 0xFF0B5F58u) : tbPal.gold;
                pillSec = tbLight ? 0xFF334B57u : 0xFFD4E5EEu;
                singleTxt = tbLight ? 0xFF0F1B22u : 0xFFF4F8FBu;
            }

            gdip::DrawTextBox(g, Tr(settings_.language, StrId::LivePreviewLabel), family, 12.2f * s, false, mockTxt,
                              16.0f * s, 0.0f, 260.0f * s, h, gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            wchar_t clockBuf[64];
            swprintf_s(clockBuf, L"^   Wi-Fi  %%71   %02d:%02d", now.hour, now.minute);
            float trayRightW = 160.0f * s;
            gdip::DrawTextBox(g, clockBuf, family, 11.8f * s, false, mockTxt,
                              w - trayRightW - 14.0f * s, 0.0f, trayRightW, h,
                              gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);

            float pillW = std::clamp(static_cast<float>(MeasureDesiredPillWidth(disp, settings_, 1.0)) * s, 64.0f * s, 235.0f * s);
            float pillH = h - 12.0f * s;
            float pillX = w - trayRightW - 22.0f * s - pillW;
            float pillY = 6.0f * s;
            gdip::FillRoundedRect(g, pillX, pillY, pillW, pillH, 6.0f * s, pillBg);
            if (drawPillBdr) {
                gdip::StrokeRoundedRect(g, pillX, pillY, pillW, pillH, 6.0f * s, pillBdr, 1.2f);
            }

            if (disp.hasLine2) {
                float tx = pillX + 4.0f * s;
                float tw = std::max(10.0f, pillW - 8.0f * s);
                int align = gdip::kStringAlignmentCenter;
                if (settings_.showTaskbarLogo) {
                    float iconSz = 22.0f * s;
                    float iconX = pillX + 6.5f * s;
                    float iconY = pillY + (pillH - 4.0f * s - iconSz) * 0.5f;
                    theme::DrawBrandLogoTile(g, iconX, iconY, iconSz);
                    tx = iconX + iconSz + 6.0f * s;
                    tw = std::max(10.0f, pillX + pillW - tx - 5.0f * s);
                    align = gdip::kStringAlignmentNear;
                }
                gdip::DrawTextBox(g, disp.line1, family, 10.6f * s, true, pillSec,
                                  tx, pillY + 2.0f * s, tw, (pillH - 6.0f * s) * 0.48f,
                                  align, gdip::kStringAlignmentCenter);
                gdip::DrawTextBox(g, disp.line2, family, 12.6f * s, true, pillPrim,
                                  tx, pillY + (pillH - 6.0f * s) * 0.45f, tw, (pillH - 6.0f * s) * 0.50f,
                                  align, gdip::kStringAlignmentCenter);
            } else {
                std::wstring txt = (settings_.showTaskbarLogo && disp.showPhaseIcon)
                    ? (L"◐ " + disp.line1)
                    : ((settings_.showTaskbarLogo && disp.showDot) ? (L"● " + disp.line1) : disp.line1);
                gdip::DrawTextBox(g, txt, family, 12.5f * s, true, disp.isUrgent ? pillPrim : singleTxt,
                                  pillX + 4.0f * s, pillY, pillW - 8.0f * s, pillH - 4.0f * s,
                                  gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
            }

            if (settings_.showProgressBar) {
                float bw = pillW - 14.0f * s;
                gdip::FillRoundedRect(g, pillX + 7.0f * s, pillY + pillH - 4.5f * s, bw, 2.2f * s, 1.1f * s,
                                      tbLight ? 0x26000000u : 0x3AFFFFFFu);
                gdip::FillRoundedRect(g, pillX + 7.0f * s, pillY + pillH - 4.5f * s,
                                      std::max(6.0f * s, bw * static_cast<float>(disp.progress)), 2.2f * s, 1.1f * s, accent);
            }

            if (family) gdip::GdipDeleteFontFamily(family);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gdiBmp);
    }

    BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDc);
}

void SettingsWindow::StartAutoDetectLocation() {
    EnableWindow(detectLocBtn_, FALSE);
    SetWindowTextW(autoLocStatus_, Tr(settings_.language, StrId::LocDetecting));
    HWND target = hwnd_;
    unsigned int requestGeneration = ++locationRequestGeneration_;

    std::thread([target, requestGeneration]() {
        auto detected = ScheduleService::DetectCurrentLocation();
        auto* heapPtr = detected ? new DetectedLocationResult(*detected) : nullptr;
        if (!PostMessageW(target, kMsgLocDetected, static_cast<WPARAM>(requestGeneration),
                          reinterpret_cast<LPARAM>(heapPtr))) {
            delete heapPtr;
        }
    }).detach();
}

void SettingsWindow::StartManualSync() {
    EnableWindow(syncBtn_, FALSE);
    SetWindowTextW(syncStatus_, Tr(settings_.language, StrId::SyncingText));
    HWND target = hwnd_;
    AppSettings copy = settings_;
    ScheduleService* svc = &scheduleService_;

    std::thread([target, svc, copy]() {
        bool ok = svc->ForceRefreshMonth(ScheduleService::GetCurrentLocalDateTime().date, copy);
        PostMessageW(target, kMsgSyncFinished, ok ? 1 : 0, 0);
    }).detach();
}

void SettingsWindow::HandleCommand(WORD id, WORD code, HWND ctrl) {
    if (isLoading_) return;

    if (id >= IDC_TAB_0 && id <= IDC_TAB_5) {
        if (code == BN_CLICKED) SwitchTab(id - IDC_TAB_0);
        return;
    }

    if (id == IDC_OK_BTN && code == BN_CLICKED) {
        CloseNow();
        return;
    }

    // Tab 0
    if (id == IDC_STYLE_COMBO && code == CBN_SELCHANGE) {
        auto chosen = static_cast<TaskbarDisplayStyle>(GetComboSelectedData(styleCombo_, 0));
        if (LicenseManager::IsFeatureProOnly(chosen) && !licenseManager_.IsProActive()) {
            isLoading_ = true;
            SetComboSelectionByData(styleCombo_, static_cast<int>(settings_.displayStyle));
            isLoading_ = false;
            OpenProTab(true);
            return;
        }
        settings_.displayStyle = chosen;
        UpdateTab0CustomTemplateVisibility();
        SaveAndNotify();
        return;
    }
    if (id >= IDC_PRESET_BASE && id < IDC_PRESET_BASE + 6 && code == BN_CLICKED) {
        auto chosen = kQuickStyles[id - IDC_PRESET_BASE];
        if (LicenseManager::IsFeatureProOnly(chosen) && !licenseManager_.IsProActive()) {
            OpenProTab(true);
            return;
        }
        settings_.displayStyle = chosen;
        isLoading_ = true;
        SetComboSelectionByData(styleCombo_, static_cast<int>(settings_.displayStyle));
        isLoading_ = false;
        UpdateTab0CustomTemplateVisibility();
        SaveAndNotify();
        return;
    }
    if (id == IDC_CUSTOM_EDIT && code == EN_CHANGE) {
        settings_.customTemplate = GetEditText(customEdit_);
        SaveAndNotify();
        return;
    }
    if (id >= IDC_TOKEN_BASE && id < IDC_TOKEN_BASE + 7 && code == BN_CLICKED) {
        if (!licenseManager_.IsProActive()) {
            OpenProTab(true);
            return;
        }
        settings_.displayStyle = TaskbarDisplayStyle::Custom;
        std::wstring cur = GetEditText(customEdit_);
        cur += kTokens[id - IDC_TOKEN_BASE];
        settings_.customTemplate = cur;
        isLoading_ = true;
        SetComboSelectionByData(styleCombo_, static_cast<int>(TaskbarDisplayStyle::Custom));
        SetWindowTextW(customEdit_, cur.c_str());
        isLoading_ = false;
        UpdateTab0CustomTemplateVisibility();
        SaveAndNotify();
        return;
    }
    if (id == IDC_FLYOUT_COMBO && code == CBN_SELCHANGE) {
        auto chosen = static_cast<FlyoutDesignStyle>(GetComboSelectedData(flyoutCombo_, 0));
        if (LicenseManager::IsFeatureProOnly(chosen) && !licenseManager_.IsProActive()) {
            isLoading_ = true;
            SetComboSelectionByData(flyoutCombo_, static_cast<int>(settings_.flyoutDesign));
            isLoading_ = false;
            OpenProTab(true);
            return;
        }
        settings_.flyoutDesign = chosen;
        SaveAndNotify();
        return;
    }
    if ((id == IDC_FLYOUT_SIZE_SM || id == IDC_FLYOUT_SIZE_MD || id == IDC_FLYOUT_SIZE_LG) && code == BN_CLICKED) {
        int pct = (id == IDC_FLYOUT_SIZE_SM) ? 95 : (id == IDC_FLYOUT_SIZE_LG ? 145 : 120);
        settings_.flyoutScalePercent = pct;
        isLoading_ = true;
        SendMessageW(flyoutScaleSlider_, TBM_SETPOS, TRUE, pct);
        wchar_t fsBuf[16];
        swprintf_s(fsBuf, L"%%%d", pct);
        SetWindowTextW(flyoutScaleLabel_, fsBuf);
        isLoading_ = false;
        SaveAndNotify();
        InvalidateRect(GetDlgItem(hwnd_, IDC_FLYOUT_SIZE_SM), nullptr, FALSE);
        InvalidateRect(GetDlgItem(hwnd_, IDC_FLYOUT_SIZE_MD), nullptr, FALSE);
        InvalidateRect(GetDlgItem(hwnd_, IDC_FLYOUT_SIZE_LG), nullptr, FALSE);
        return;
    }
    if (id == IDC_DOCK_COMBO && code == CBN_SELCHANGE) {
        settings_.dockPosition = static_cast<TaskbarDockPosition>(GetComboSelectedData(dockCombo_, 0));
        SaveAndNotify();
        return;
    }
    if (id == IDC_OFFSET_RESET && code == BN_CLICKED) {
        SendMessageW(offsetSlider_, TBM_SETPOS, TRUE, 0);
        settings_.horizontalOffsetPx = 0;
        SetWindowTextW(offsetLabel_, L"0 px");
        SaveAndNotify();
        return;
    }

    if ((id == IDC_SHOW_PILL_CHK || id == IDC_OPAQUE_CHK || id == IDC_SHOW_TRAY_CHK ||
         id == IDC_SHOW_BAR_CHK || id == IDC_SHOW_SEC_CHK || id == IDC_ACCENT_CHK ||
         id == IDC_TASKBAR_FRAME_CHK || id == IDC_TASKBAR_LOGO_CHK) && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(id);
        if (id == IDC_SHOW_PILL_CHK && !nextVal && !settings_.showTrayIcon) return;
        if (id == IDC_SHOW_TRAY_CHK && !nextVal && !settings_.showTaskbarPill) return;

        SetFluentCheck(id, ctrl, nextVal);
        settings_.showTaskbarPill = GetFluentCheck(IDC_SHOW_PILL_CHK);
        settings_.opaqueTaskbarBackground = GetFluentCheck(IDC_OPAQUE_CHK);
        settings_.showTrayIcon = GetFluentCheck(IDC_SHOW_TRAY_CHK);
        settings_.showProgressBar = GetFluentCheck(IDC_SHOW_BAR_CHK);
        settings_.showSeconds = GetFluentCheck(IDC_SHOW_SEC_CHK);
        settings_.useSystemAccentColor = GetFluentCheck(IDC_ACCENT_CHK);
        settings_.showTaskbarFrame = GetFluentCheck(IDC_TASKBAR_FRAME_CHK);
        settings_.showTaskbarLogo = GetFluentCheck(IDC_TASKBAR_LOGO_CHK);
        SaveAndNotify();
        if (id == IDC_ACCENT_CHK) {
            PostMessageW(hwnd_, kMsgRebuildUi, 0, 0);
        }
        return;
    }
    if ((id == IDC_TRAY_COMBO || id == IDC_FONT_COMBO ||
         id == IDC_URGENT_COMBO || id == IDC_THEME_COMBO) && code == CBN_SELCHANGE) {
        settings_.trayStyle = static_cast<TrayIconStyle>(GetComboSelectedData(trayCombo_, 0));
        settings_.fontSize = static_cast<FontSizePreset>(GetComboSelectedData(fontCombo_, 1));
        settings_.highlightUrgentMinutes = GetComboSelectedData(urgentCombo_, 15);
        auto oldTheme = settings_.themeMode;
        settings_.themeMode = static_cast<AppThemeMode>(GetComboSelectedData(themeCombo_, 0));
        SaveAndNotify();
        if (oldTheme != settings_.themeMode) {
            PostMessageW(hwnd_, kMsgRebuildUi, 0, 0);
        }
        return;
    }

    // Tab 1
    if (id == IDC_DETECT_LOC_BTN && code == BN_CLICKED) {
        StartAutoDetectLocation();
        return;
    }
    if (id == IDC_AUTO_DETECT_LOC_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_AUTO_DETECT_LOC_CHK);
        ++locationRequestGeneration_;
        settings_.autoDetectLocation = nextVal;
        SetFluentCheck(IDC_AUTO_DETECT_LOC_CHK, ctrl, nextVal);
        if (nextVal) {
            settings_.useCustomCoordinates = false;
            SetFluentCheck(IDC_CUSTOM_COORD_CHK, customCoordChk_, false);
            EnableWindow(latEdit_, FALSE);
            EnableWindow(lonEdit_, FALSE);
            EnableWindow(applyCoordBtn_, FALSE);
        }
        SaveAndNotify();
        if (nextVal) StartAutoDetectLocation();
        return;
    }
    if (id == IDC_AUTO_METHOD_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_AUTO_METHOD_CHK);
        SetFluentCheck(IDC_AUTO_METHOD_CHK, ctrl, nextVal);
        settings_.autoSelectMethodByCountry = nextVal;
        SaveAndNotify();
        return;
    }
    if (id == IDC_CITY_SEARCH_EDIT && code == EN_CHANGE) {
        std::wstring query = GetEditText(citySearchEdit_);
        RefreshCityList(query);
        KillTimer(hwnd_, kTimerWorldwideCitySearch);
        if (query.size() >= 3) SetTimer(hwnd_, kTimerWorldwideCitySearch, 450, nullptr);
        return;
    }
    if (id == IDC_CITY_LIST && code == LBN_SELCHANGE) {
        int idx = static_cast<int>(SendMessageW(cityList_, LB_GETCURSEL, 0, 0));
        if (idx >= 0 && static_cast<size_t>(idx) < visibleCities_.size()) {
            const auto& c = visibleCities_[static_cast<size_t>(idx)];
            settings_.cityId = c.id;
            settings_.cityName = c.name;
            settings_.countryName = c.country;
            settings_.latitude = c.latitude;
            settings_.longitude = c.longitude;
            settings_.timeZoneId = c.timeZoneId;
            settings_.useCustomCoordinates = false;
            settings_.autoDetectLocation = false;
            ++locationRequestGeneration_;

            isLoading_ = true;
            SetFluentCheck(IDC_AUTO_DETECT_LOC_CHK, autoDetectLocChk_, false);
            SetFluentCheck(IDC_CUSTOM_COORD_CHK, customCoordChk_, false);
            EnableWindow(latEdit_, FALSE);
            EnableWindow(lonEdit_, FALSE);
            EnableWindow(applyCoordBtn_, FALSE);
            wchar_t latBuf[32], lonBuf[32];
            swprintf_s(latBuf, L"%.4f", c.latitude);
            swprintf_s(lonBuf, L"%.4f", c.longitude);
            SetWindowTextW(latEdit_, latBuf);
            SetWindowTextW(lonEdit_, lonBuf);
            if (settings_.autoSelectMethodByCountry) {
                settings_.calculationMethod = GetRecommendedMethodForCountry(c.countryCode);
                SetComboSelectionByData(methodCombo_, static_cast<int>(settings_.calculationMethod));
            }
            isLoading_ = false;
            SaveAndNotify();
        }
        return;
    }
    if (id == IDC_CUSTOM_COORD_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_CUSTOM_COORD_CHK);
        SetFluentCheck(IDC_CUSTOM_COORD_CHK, ctrl, nextVal);
        ++locationRequestGeneration_;
        settings_.useCustomCoordinates = nextVal;
        if (nextVal) {
            settings_.autoDetectLocation = false;
            SetFluentCheck(IDC_AUTO_DETECT_LOC_CHK, autoDetectLocChk_, false);
        }
        EnableWindow(latEdit_, nextVal);
        EnableWindow(lonEdit_, nextVal);
        EnableWindow(applyCoordBtn_, nextVal);
        SaveAndNotify();
        return;
    }
    if (id == IDC_APPLY_COORD_BTN && code == BN_CLICKED) {
        double lat = 0.0, lon = 0.0;
        if (ParseDoubleInvariant(GetEditText(latEdit_), lat) &&
            ParseDoubleInvariant(GetEditText(lonEdit_), lon)) {
            ++locationRequestGeneration_;
            settings_.latitude = std::clamp(lat, -90.0, 90.0);
            settings_.longitude = std::clamp(lon, -180.0, 180.0);
            settings_.useCustomCoordinates = true;
            settings_.autoDetectLocation = false;
            SetFluentCheck(IDC_AUTO_DETECT_LOC_CHK, autoDetectLocChk_, false);
            wchar_t nameBuf[64];
            swprintf_s(nameBuf, L"%.2f, %.2f", settings_.latitude, settings_.longitude);
            settings_.cityName = nameBuf;
            settings_.countryName = L"Custom";
            SaveAndNotify();
        }
        return;
    }
    if ((id == IDC_METHOD_COMBO || id == IDC_ASR_COMBO) && code == CBN_SELCHANGE) {
        settings_.calculationMethod = static_cast<CalculationMethod>(GetComboSelectedData(methodCombo_, 13));
        settings_.asrMethod = static_cast<AsrJuristicMethod>(GetComboSelectedData(asrCombo_, 0));
        SaveAndNotify();
        return;
    }
    if (id == IDC_ONLINE_SYNC_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_ONLINE_SYNC_CHK);
        SetFluentCheck(IDC_ONLINE_SYNC_CHK, ctrl, nextVal);
        settings_.useOnlineSync = nextVal;
        SaveAndNotify();
        return;
    }
    if (id == IDC_SYNC_NOW_BTN && code == BN_CLICKED) {
        StartManualSync();
        return;
    }
    if (id == IDC_OPEN_IMSAKIYE_BTN && code == BN_CLICKED) {
        if (onOpenImsakiye_) onOpenImsakiye_();
        return;
    }
    if ((id == IDC_APPLY_OFFSETS_BTN && code == BN_CLICKED) ||
        (id >= IDC_OFFSET_EDIT_BASE && id < IDC_OFFSET_EDIT_BASE + 6 && code == EN_KILLFOCUS)) {
        settings_.offsets.imsak = ParseIntClamped(GetEditText(offsetEdits_[0]), -60, 60, 0);
        settings_.offsets.gunes = ParseIntClamped(GetEditText(offsetEdits_[1]), -60, 60, 0);
        settings_.offsets.ogle = ParseIntClamped(GetEditText(offsetEdits_[2]), -60, 60, 0);
        settings_.offsets.ikindi = ParseIntClamped(GetEditText(offsetEdits_[3]), -60, 60, 0);
        settings_.offsets.aksam = ParseIntClamped(GetEditText(offsetEdits_[4]), -60, 60, 0);
        settings_.offsets.yatsi = ParseIntClamped(GetEditText(offsetEdits_[5]), -60, 60, 0);
        SaveAndNotify();
        return;
    }

    // Tab 2 & 3
    if (id >= IDC_ALERT_TIME_BASE && id < IDC_ALERT_TIME_BASE + 6 && code == BN_CLICKED) {
        int idx = id - IDC_ALERT_TIME_BASE;
        bool nextVal = !GetFluentCheck(id);
        SetFluentCheck(id, ctrl, nextVal);
        settings_.alerts.GetRule(static_cast<PrayerKind>(idx)).notifyAtTime = nextVal;
        SaveAndNotify();
        return;
    }
    if (id >= IDC_ALERT_PRE_BASE && id < IDC_ALERT_PRE_BASE + 6 && code == CBN_SELCHANGE) {
        int idx = id - IDC_ALERT_PRE_BASE;
        settings_.alerts.GetRule(static_cast<PrayerKind>(idx)).preNotifyMinutes =
            GetComboSelectedData(alertPreCombo_[idx], 15);
        SaveAndNotify();
        return;
    }
    if (id >= IDC_ALERT_SOUND_BASE && id < IDC_ALERT_SOUND_BASE + 6 && code == BN_CLICKED) {
        int idx = id - IDC_ALERT_SOUND_BASE;
        bool nextVal = !GetFluentCheck(id);
        SetFluentCheck(id, ctrl, nextVal);
        settings_.alerts.GetRule(static_cast<PrayerKind>(idx)).playSound = nextVal;
        SaveAndNotify();
        return;
    }
    if ((id == IDC_ENTRY_SOUND_COMBO || id == IDC_PRE_SOUND_COMBO || id == IDC_FAJR_SOUND_COMBO) && code == CBN_SELCHANGE) {
        settings_.entrySound = static_cast<NotificationSoundKind>(GetComboSelectedData(entrySoundCombo_, 3));
        settings_.preNotifySound = static_cast<NotificationSoundKind>(GetComboSelectedData(preSoundCombo_, 1));
        settings_.fajrSound = GetComboSelectedData(fajrSoundCombo_, -1);
        SaveAndNotify();
        return;
    }
    if (id == IDC_HOTKEY_BTN && code == BN_CLICKED) {
        BeginHotkeyCapture();
        return;
    }
    struct FeatureToggle { WORD id; bool AppSettings::*field; };
    static constexpr FeatureToggle kFeatureToggles[] = {
        {IDC_HOLIDAY_CHK, &AppSettings::holidayReminders},
        {IDC_EXTRA_TIMES_CHK, &AppSettings::showExtraTimes},
        {IDC_TRAVEL_CHK, &AppSettings::travelMode},
        {IDC_TRACKING_CHK, &AppSettings::prayerTracking},
        {IDC_TOAST_CHK, &AppSettings::modernToasts},
        {IDC_HOTKEY_CHK, &AppSettings::globalHotkey},
    };
    for (const auto& t : kFeatureToggles) {
        if (id == t.id && code == BN_CLICKED) {
            bool nextVal = !GetFluentCheck(id);
            SetFluentCheck(id, ctrl, nextVal);
            settings_.*(t.field) = nextVal;
            SaveAndNotify();
            return;
        }
    }
    if (id == IDC_BROWSE_SOUND_BTN && code == BN_CLICKED) {
        if (!licenseManager_.IsProActive()) {
            OpenProTab(true);
            return;
        }
        wchar_t fileBuf[MAX_PATH]{};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"Audio (*.mp3;*.wav)\0*.mp3;*.wav\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
            settings_.customSoundPath = fileBuf;
            SetWindowTextW(customSoundEdit_, fileBuf);
            SaveAndNotify();
        }
        return;
    }
    if (id == IDC_TEST_ENTRY_BTN && code == BN_CLICKED) {
        AudioPlayer::Play(settings_.entrySound, settings_.customSoundPath, settings_.soundVolumePercent);
        return;
    }
    if (id == IDC_TEST_PRE_BTN && code == BN_CLICKED) {
        AudioPlayer::Play(settings_.preNotifySound, settings_.customSoundPath, settings_.soundVolumePercent);
        return;
    }
    if (id == IDC_STOP_SOUND_BTN && code == BN_CLICKED) {
        AudioPlayer::Stop();
        return;
    }
    if (id == IDC_TEST_NOTIF_BTN && code == BN_CLICKED) {
        bool playAudio = settings_.preNotifySound != NotificationSoundKind::None;
        if (playAudio) {
            AudioPlayer::Play(settings_.preNotifySound, settings_.customSoundPath, settings_.soundVolumePercent);
        }
        if (onTestNotification_) {
            onTestNotification_(
                Tr(settings_.language, StrId::TestNotifTitle),
                Tr(settings_.language, StrId::TestNotifBody),
                playAudio);
        }
        return;
    }
    if (id == IDC_QUIET_FULLSCREEN_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_QUIET_FULLSCREEN_CHK);
        SetFluentCheck(IDC_QUIET_FULLSCREEN_CHK, ctrl, nextVal);
        settings_.quietDuringFullscreen = nextVal;
        SaveAndNotify();
        return;
    }
    if (id == IDC_KERAHAT_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_KERAHAT_CHK);
        SetFluentCheck(IDC_KERAHAT_CHK, ctrl, nextVal);
        settings_.showKerahatIndicator = nextVal;
        SaveAndNotify();
        return;
    }
    if (id == IDC_RAMADAN_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_RAMADAN_CHK);
        SetFluentCheck(IDC_RAMADAN_CHK, ctrl, nextVal);
        settings_.ramadanIftarSahurMode = nextVal;
        SaveAndNotify();
        return;
    }
    if (id == IDC_FRIDAY_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_FRIDAY_CHK);
        SetFluentCheck(IDC_FRIDAY_CHK, ctrl, nextVal);
        settings_.fridayReminder = nextVal;
        SaveAndNotify();
        return;
    }

    // Tab 4 (Dil & Sistem)
    if (id == IDC_LANG_COMBO && code == CBN_SELCHANGE) {
        settings_.language = static_cast<AppLanguage>(GetComboSelectedData(langCombo_, 1));
        SaveAndNotify();
        PostMessageW(hwnd_, kMsgRebuildUi, 0, 0);
        return;
    }
    if (id == IDC_APPLY_NAMES_BTN && code == BN_CLICKED) {
        bool anyCustom = false;
        for (int i = 0; i < 6; ++i) {
            if (!TrimWide(GetEditText(customNameEdits_[i])).empty() ||
                !TrimWide(GetEditText(customShortEdits_[i])).empty()) {
                anyCustom = true;
            }
        }
        if (anyCustom && !licenseManager_.IsProActive()) {
            OpenProTab(true);
            return;
        }
        for (int i = 0; i < 6; ++i) {
            settings_.customPrayerNames[static_cast<size_t>(i)] = TrimWide(GetEditText(customNameEdits_[i]));
            settings_.customPrayerShortCodes[static_cast<size_t>(i)] = TrimWide(GetEditText(customShortEdits_[i]));
        }
        SaveAndNotify();
        PostMessageW(hwnd_, kMsgRebuildUi, 0, 0);
        return;
    }
    if (id == IDC_RESET_NAMES_BTN && code == BN_CLICKED) {
        for (int i = 0; i < 6; ++i) {
            settings_.customPrayerNames[static_cast<size_t>(i)].clear();
            settings_.customPrayerShortCodes[static_cast<size_t>(i)].clear();
            SetWindowTextW(customNameEdits_[i], L"");
            SetWindowTextW(customShortEdits_[i], L"");
        }
        SaveAndNotify();
        PostMessageW(hwnd_, kMsgRebuildUi, 0, 0);
        return;
    }
    if (id == IDC_STARTUP_CHK && code == BN_CLICKED) {
        bool want = !GetFluentCheck(IDC_STARTUP_CHK);
        std::wstring note;
        if (SetStartupEnabled(want, note)) {
            SetFluentCheck(IDC_STARTUP_CHK, ctrl, want);
            settings_.startWithWindows = want;
            SaveAndNotify();
        }
        return;
    }
    if (id == IDC_AUTO_MEM_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_AUTO_MEM_CHK);
        SetFluentCheck(IDC_AUTO_MEM_CHK, ctrl, nextVal);
        settings_.autoOptimizeMemory = nextVal;
        SaveAndNotify();
        if (nextVal) {
            TrimWorkingSet();
            RefreshPreviewAndMemory();
        }
        return;
    }
    if (id == IDC_EXPORT_BTN && code == BN_CLICKED) {
        if (!licenseManager_.IsProActive()) {
            OpenProTab(true);
            return;
        }
        wchar_t fileBuf[MAX_PATH] = L"prayerbar_settings.json";
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"json";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (GetSaveFileNameW(&ofn) && settingsStore_.ExportToFile(settings_, fileBuf)) {
            SetWindowTextW(sysStatusLabel_, Tr(settings_.language, StrId::MsgSettingsExported));
        }
        return;
    }
    if (id == IDC_IMPORT_BTN && code == BN_CLICKED) {
        if (!licenseManager_.IsProActive()) {
            OpenProTab(true);
            return;
        }
        wchar_t fileBuf[MAX_PATH]{};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
            if (auto loaded = settingsStore_.ImportFromFile(fileBuf)) {
                settings_ = *loaded;
                SaveAndNotify();
                PostMessageW(hwnd_, kMsgRebuildUi, 0, 0);
            }
        }
        return;
    }
    if (id == IDC_OPEN_PRO_BTN && code == BN_CLICKED) {
        OpenProTab(false);
        return;
    }

    // Tab 5 (Pro & Lisans)
    if (id == IDC_PRO_BUY_ANNUAL && code == BN_CLICKED) {
        StartProStorePurchase(commercial::kStoreAddonAnnualId);
        return;
    }
    if (id == IDC_PRO_BUY_LIFETIME && code == BN_CLICKED) {
        StartProStorePurchase(commercial::kStoreAddonLifetimeId);
        return;
    }
    if (id == IDC_PRO_RESTORE_BTN && code == BN_CLICKED) {
        StartProStoreRestore();
        return;
    }
    if (id == IDC_PRO_MANAGE_BTN && code == BN_CLICKED) {
        ShellExecuteW(hwnd_, L"open", commercial::kMicrosoftAccountSubsUri, nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
}

LRESULT CALLBACK SettingsWindow::WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<SettingsWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    return self ? self->WndProc(hWnd, msg, wParam, lParam) : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT SettingsWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
            if (wParam == kTimerWorldwideCitySearch) {
                KillTimer(hWnd, kTimerWorldwideCitySearch);
                StartWorldwideCitySearch(GetEditText(citySearchEdit_));
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hWnd, &ps);
            RECT rc{};
            GetClientRect(hWnd, &rc);
            PaintBackgroundAndCards(hdc, rc.right - rc.left, rc.bottom - rc.top);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            HWND ctrl = reinterpret_cast<HWND>(lParam);
            bool inCard = cardStaticLabels_.count(ctrl) > 0;
            bool muted = mutedStaticLabels_.count(ctrl) > 0;
            auto pal = theme::GetUiPalette(isDark_, settings_.useSystemAccentColor);
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, theme::ArgbToColorRef(inCard ? pal.bgCard : pal.bgWindow));
            SetTextColor(hdc, theme::ArgbToColorRef(muted ? pal.textSecondary : pal.textPrimary));
            return reinterpret_cast<LRESULT>(inCard ? cardBrush_ : bgBrush_);
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            auto pal = theme::GetUiPalette(isDark_, settings_.useSystemAccentColor);
            SetBkColor(hdc, theme::ArgbToColorRef(pal.bgInput));
            SetTextColor(hdc, theme::ArgbToColorRef(pal.textPrimary));
            return reinterpret_cast<LRESULT>(editBrush_);
        }

        case WM_DRAWITEM: {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (!dis) break;
            if (dis->CtlID == IDC_PREVIEW_BOX) {
                PaintTaskbarPreview(dis->hDC, dis->rcItem.right - dis->rcItem.left, dis->rcItem.bottom - dis->rcItem.top);
                return TRUE;
            }
            if (dis->CtlID >= IDC_TAB_0 && dis->CtlID <= IDC_TAB_5) {
                PaintSidebarTab(dis, dis->CtlID - IDC_TAB_0);
                return TRUE;
            }
            WORD id = static_cast<WORD>(dis->CtlID);
            if (checkStates_.count(id) > 0) {
                PaintOwnerCheckbox(dis);
                return TRUE;
            }
            PaintOwnerPushButton(dis);
            return TRUE;
        }

        case WM_HSCROLL: {
            HWND slider = reinterpret_cast<HWND>(lParam);
            if (!isLoading_ && slider == offsetSlider_) {
                int px = static_cast<int>(SendMessageW(offsetSlider_, TBM_GETPOS, 0, 0));
                settings_.horizontalOffsetPx = px;
                wchar_t buf[32];
                swprintf_s(buf, L"%d px", px);
                SetWindowTextW(offsetLabel_, buf);
                SaveAndNotify();
            } else if (!isLoading_ && slider == flyoutScaleSlider_) {
                int pct = std::clamp(static_cast<int>(SendMessageW(flyoutScaleSlider_, TBM_GETPOS, 0, 0)), 80, 200);
                settings_.flyoutScalePercent = pct;
                wchar_t buf[16];
                swprintf_s(buf, L"%%%d", pct);
                SetWindowTextW(flyoutScaleLabel_, buf);
                SaveAndNotify();
                InvalidateRect(GetDlgItem(hwnd_, IDC_FLYOUT_SIZE_SM), nullptr, FALSE);
                InvalidateRect(GetDlgItem(hwnd_, IDC_FLYOUT_SIZE_MD), nullptr, FALSE);
                InvalidateRect(GetDlgItem(hwnd_, IDC_FLYOUT_SIZE_LG), nullptr, FALSE);
            } else if (!isLoading_ && slider == volumeSlider_) {
                int vol = static_cast<int>(SendMessageW(volumeSlider_, TBM_GETPOS, 0, 0));
                settings_.soundVolumePercent = vol;
                wchar_t buf[16];
                swprintf_s(buf, L"%%%d", vol);
                SetWindowTextW(volumeLabel_, buf);
                SaveAndNotify();
            }
            return 0;
        }

        case WM_COMMAND:
            HandleCommand(LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HWND>(lParam));
            return 0;

        case kMsgRebuildUi:
            RebuildUiAfterChange();
            return 0;

        case kMsgCitySearchResults: {
            auto* result = reinterpret_cast<CitySearchResults*>(lParam);
            if (!result) return 0;
            if (result->query == GetEditText(citySearchEdit_)) {
                for (auto& city : result->cities) {
                    bool duplicate = std::any_of(visibleCities_.begin(), visibleCities_.end(), [&](const CityLocation& existing) {
                        return std::abs(existing.latitude - city.latitude) < 0.02 &&
                               std::abs(existing.longitude - city.longitude) < 0.02;
                    });
                    if (!duplicate) visibleCities_.push_back(std::move(city));
                }

                SendMessageW(cityList_, LB_RESETCONTENT, 0, 0);
                int selIdx = -1;
                for (size_t i = 0; i < visibleCities_.size(); ++i) {
                    std::wstring label = visibleCities_[i].DisplayLabel();
                    SendMessageW(cityList_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                    if (visibleCities_[i].id == settings_.cityId) selIdx = static_cast<int>(i);
                }
                if (selIdx >= 0) SendMessageW(cityList_, LB_SETCURSEL, selIdx, 0);
            }
            delete result;
            return 0;
        }

        case kMsgLocDetected: {
            auto* detected = reinterpret_cast<DetectedLocationResult*>(lParam);
            if (static_cast<unsigned int>(wParam) != locationRequestGeneration_) {
                EnableWindow(detectLocBtn_, TRUE);
                delete detected;
                return 0;
            }
            EnableWindow(detectLocBtn_, TRUE);
            if (!detected) {
                SetWindowTextW(autoLocStatus_, Tr(settings_.language, StrId::LocDetectFailed));
                return 0;
            }

            isLoading_ = true;
            settings_.cityId = detected->cityId;
            settings_.cityName = detected->cityName;
            settings_.countryName = detected->countryName;
            settings_.latitude = detected->latitude;
            settings_.longitude = detected->longitude;
            settings_.timeZoneId = detected->timeZoneId;
            settings_.useCustomCoordinates = false;
            if (settings_.autoSelectMethodByCountry) {
                settings_.calculationMethod = detected->recommendedMethod;
                SetComboSelectionByData(methodCombo_, static_cast<int>(settings_.calculationMethod));
            }
            SetFluentCheck(IDC_CUSTOM_COORD_CHK, customCoordChk_, settings_.useCustomCoordinates);
            SetFluentCheck(IDC_AUTO_DETECT_LOC_CHK, autoDetectLocChk_, settings_.autoDetectLocation);
            EnableWindow(latEdit_, settings_.useCustomCoordinates);
            EnableWindow(lonEdit_, settings_.useCustomCoordinates);
            EnableWindow(applyCoordBtn_, settings_.useCustomCoordinates);

            wchar_t latBuf[32], lonBuf[32];
            swprintf_s(latBuf, L"%.4f", settings_.latitude);
            swprintf_s(lonBuf, L"%.4f", settings_.longitude);
            SetWindowTextW(latEdit_, latBuf);
            SetWindowTextW(lonEdit_, lonBuf);
            RefreshCityList(GetEditText(citySearchEdit_));
            isLoading_ = false;
            delete detected;
            SaveAndNotify();
            return 0;
        }

        case kMsgSyncFinished:
            EnableWindow(syncBtn_, TRUE);
            SetWindowTextW(syncStatus_, wParam ? Tr(settings_.language, StrId::SyncOkText)
                                               : Tr(settings_.language, StrId::SyncOfflineText));
            RefreshPreviewAndMemory();
            return 0;

        case kMsgStorePurchaseDone: {
            SetProBusy(false);
            auto* resp = reinterpret_cast<StorePurchaseResponse*>(lParam);
            if (resp) {
                AppLanguage lang = settings_.language;
                switch (resp->outcome) {
                    case StorePurchaseOutcome::Succeeded:
                    case StorePurchaseOutcome::AlreadyPurchased:
                        delete resp;
                        StartProStoreRestore();
                        return 0;
                    case StorePurchaseOutcome::NotPurchased:
                        proFeedbackIsError_ = false;
                        proFeedbackText_ = Tr(lang, StrId::StorePurchaseCanceled);
                        break;
                    case StorePurchaseOutcome::NotConfigured:
                        proFeedbackIsError_ = true;
                        proFeedbackText_ = Tr(lang, StrId::StoreAddonNotConfigured);
                        break;
                    case StorePurchaseOutcome::StoreUnavailable:
                        proFeedbackIsError_ = true;
                        proFeedbackText_ = Tr(lang, StrId::StoreNotPackaged);
                        break;
                    case StorePurchaseOutcome::NetworkError:
                    case StorePurchaseOutcome::ServerError:
                    default:
                        proFeedbackIsError_ = true;
                        proFeedbackText_ = Tr(lang, StrId::StorePurchaseFailed);
                        break;
                }
                delete resp;
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        case kMsgStoreRestoreDone: {
            SetProBusy(false);
            auto* snap = reinterpret_cast<StoreLicenseSnapshot*>(lParam);
            if (snap) {
                AppLanguage lang = settings_.language;
                bool changed = licenseManager_.ApplyStoreLicenseSnapshot(*snap, LicenseManager::CurrentLocalDate());
                if (!snap->storeAvailable) {
                    proFeedbackIsError_ = true;
                    proFeedbackText_ = Tr(lang, StrId::StoreNotPackaged);
                } else if (snap->hasProLifetime || snap->hasProAnnual) {
                    proLockedHint_ = false;
                    proFeedbackIsError_ = false;
                    proFeedbackText_ = Tr(lang, StrId::StorePurchasedOk);
                } else {
                    proFeedbackIsError_ = false;
                    proFeedbackText_ = Tr(lang, StrId::StoreRestoredNone);
                }
                delete snap;
                RefreshLicenseStatus();
                if (changed && onLicenseChanged_) {
                    onLicenseChanged_();
                }
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;
        }

        case WM_CLOSE:
            CloseNow();
            return 0;

        case WM_DESTROY:
            KillTimer(hWnd, kTimerWorldwideCitySearch);
            hwnd_ = nullptr;
            allCreatedControls_.clear();
            for (auto& vec : tabControls_) vec.clear();
            cardStaticLabels_.clear();
            mutedStaticLabels_.clear();
            rightToggleIds_.clear();
            checkStates_.clear();
            checkInCard_.clear();
            hoveredBtn_ = nullptr;
            for (HWND& h : presetBtns_) h = nullptr;
            customTemplateLabel_ = nullptr;
            for (HWND& h : tokenBtns_) h = nullptr;
            proRestoreBtn_ = nullptr;
            proManageBtn_ = nullptr;
            proBuyAnnualBtn_ = nullptr;
            proBuyLifetimeBtn_ = nullptr;
            return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace vakit
