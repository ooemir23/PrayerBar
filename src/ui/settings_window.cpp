#include "settings_window.h"
#include "gdiplus_flat.h"
#include "taskbar_renderer.h"
#include "../core/city_catalog.h"
#include "../core/formatter.h"
#include "../core/i18n.h"
#include "../core/prayer_calc.h"
#include "../core/utf_util.h"
#include "../platform/audio_player.h"
#include "../platform/theme_utils.h"

#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <initguid.h>
#include <oleacc.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace vakit {

namespace {

constexpr wchar_t kSettingsClassName[] = L"VakitCubuguSettingsWindow";
constexpr int kBaseW = 648;
constexpr int kBaseH = 716;

enum CtrlId : WORD {
    IDC_TAB_0 = 100,
    IDC_TAB_1 = 101,
    IDC_TAB_2 = 102,
    IDC_TAB_3 = 103,
    IDC_TAB_4 = 104,
    IDC_PREVIEW_BOX = 105,
    IDC_OK_BTN = 106,

    // Tab 0 (Görünüm)
    IDC_STYLE_COMBO = 200,
    IDC_PRESET_BASE = 201, // 201..206
    IDC_CUSTOM_EDIT = 210,
    IDC_TOKEN_BASE = 211,  // 211..217
    IDC_FLYOUT_COMBO = 218,
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

    // Tab 3 (Dil & Sistem)
    IDC_LANG_COMBO = 500,
    IDC_CUSTOM_NAME_BASE = 510,  // 510..515
    IDC_CUSTOM_SHORT_BASE = 520, // 520..525
    IDC_APPLY_NAMES_BTN = 530,
    IDC_RESET_NAMES_BTN = 531,
    IDC_STARTUP_CHK = 532,
    IDC_AUTO_MEM_CHK = 533,
    IDC_EXPORT_BTN = 534,
    IDC_IMPORT_BTN = 535,
    IDC_OPEN_PRO_BTN = 536
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
        case 0: return {{14, 110, 602, 210}, {14, 326, 602, 302}};
        case 1: return {{14, 110, 602, 248}, {14, 364, 602, 264}};
        case 2: return {{14, 110, 602, 244}, {14, 360, 602, 236}};
        case 3: return {{14, 110, 602, 234}, {14, 350, 602, 240}};
        case 4: return {{14, 110, 602, 56},  {14, 172, 602, 274}, {14, 452, 602, 176}};
        default: return {};
    }
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
    std::function<bool()> isHotkeyOk)
    : settingsStore_(settingsStore),
      scheduleService_(scheduleService),
      licenseManager_(licenseManager),
      onSettingsSaved_(std::move(onSettingsSaved)),
      onTestNotification_(std::move(onTestNotification)),
      onOpenImsakiye_(std::move(onOpenImsakiye)),
      onRequestUpgrade_(std::move(onRequestUpgrade)),
      isHotkeyOk_(std::move(isHotkeyOk)) {

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
    if (bgBrush_) DeleteObject(bgBrush_);
    if (cardBrush_) DeleteObject(cardBrush_);
    if (editBrush_) DeleteObject(editBrush_);
}

void SettingsWindow::OpenOrFocus(int tab) {
    if (tab >= 0) activeTab_ = tab;
    if (hwnd_) {
        if (tab >= 0) SwitchTab(tab);
        if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
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

    HWND tb = FindWindowW(L"Shell_TrayWnd", nullptr);
    UINT dpi = tb ? GetDpiForWindowCompat(tb) : 96;
    scale_ = dpi >= 96 ? static_cast<double>(dpi) / 96.0 : 1.0;

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

void SettingsWindow::DestroyAllControls() {
    for (HWND h : allCreatedControls_) {
        if (h && IsWindow(h)) {
            DestroyWindow(h);
        }
    }
    allCreatedControls_.clear();
    for (auto& vec : tabControls_) vec.clear();
    cardStaticLabels_.clear();
    checkStates_.clear();
    checkInCard_.clear();
}

void SettingsWindow::CloseNow() {
    if (hwnd_) {
        HWND h = hwnd_;
        hwnd_ = nullptr;
        allCreatedControls_.clear();
        for (auto& vec : tabControls_) vec.clear();
        cardStaticLabels_.clear();
        checkStates_.clear();
        checkInCard_.clear();
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
    if (!hwnd_ || !licenseLabel_) return;
    LicenseStatus lst = licenseManager_.GetStatus();
    AppLanguage lang = settings_.language;
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

void SettingsWindow::RebuildFonts() {
    if (uiFont_) DeleteObject(uiFont_);
    if (boldFont_) DeleteObject(boldFont_);
    if (smallFont_) DeleteObject(smallFont_);

    int hUi = -static_cast<int>(std::llround(13.0 * scale_));
    int hSmall = -static_cast<int>(std::llround(11.5 * scale_));
    uiFont_ = CreateWin11Font(hUi, FW_NORMAL, false);
    boldFont_ = CreateWin11Font(hUi, FW_SEMIBOLD, true);
    smallFont_ = CreateWin11Font(hSmall, FW_NORMAL, false);
}

void SettingsWindow::ApplyThemeBrushes() {
    if (bgBrush_) DeleteObject(bgBrush_);
    if (cardBrush_) DeleteObject(cardBrush_);
    if (editBrush_) DeleteObject(editBrush_);

    isDark_ = !IsAppLight(settings_.themeMode);
    if (isDark_) {
        bgBrush_ = CreateSolidBrush(RGB(32, 32, 32));
        cardBrush_ = CreateSolidBrush(RGB(43, 43, 46));
        editBrush_ = CreateSolidBrush(RGB(50, 50, 56));
    } else {
        bgBrush_ = CreateSolidBrush(RGB(243, 243, 243));
        cardBrush_ = CreateSolidBrush(RGB(255, 255, 255));
        editBrush_ = CreateSolidBrush(RGB(255, 255, 255));
    }
    if (hwnd_) {
        ApplyWindowDwmStyle(hwnd_, isDark_, 2, 2);
    }
}

namespace {
// Dynamic annotation (no custom IAccessible): owner-drawn BUTTONs otherwise read as plain push buttons.
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
           id == IDC_OPEN_PRO_BTN;
}

void SettingsWindow::CreateControls() {
    auto S = [&](int v) -> int { return static_cast<int>(std::llround(v * scale_)); };
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    AppLanguage lang = settings_.language;

    auto isInTabCard = [&](int tab, int x, int y, int w, int h) -> bool {
        if (tab < 0 || tab > 3) return false;
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
        if (tab >= 0 && tab < 5) tabControls_[tab].push_back(c);
    };

    auto makeLabel = [&](int tab, int x, int y, int w, int h, const wchar_t* txt, bool bold = false, bool small = false) -> HWND {
        HWND c = CreateWindowExW(0, L"STATIC", txt, WS_CHILD | SS_LEFT | SS_NOPREFIX,
                                 S(x), S(y), S(w), S(h), hwnd_, nullptr, hInst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? boldFont_ : (small ? smallFont_ : uiFont_)), TRUE);
        if (isInTabCard(tab, x, y, w, h)) cardStaticLabels_.insert(c);
        track(tab, c);
        return c;
    };

    auto makeBtn = [&](int tab, WORD id, int x, int y, int w, int h, const wchar_t* txt) -> HWND {
        HWND c = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                                 S(x), S(y), S(w), S(h),
                                 hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), hInst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
        checkInCard_[id] = isInTabCard(tab, x, y, w, h);
        track(tab, c);
        return c;
    };

    auto makeChk = [&](int tab, WORD id, int x, int y, int w, int h, const wchar_t* txt) -> HWND {
        HWND c = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                                 S(x), S(y), S(w), S(h),
                                 hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), hInst, nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont_), TRUE);
        checkStates_[id] = false;
        checkInCard_[id] = isInTabCard(tab, x, y, w, h);
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

    // Ortak üst önizleme ve 4 tab
    previewBox_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                  S(14), S(10), S(602), S(54), hwnd_,
                                  reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_PREVIEW_BOX)), hInst, nullptr);
    allCreatedControls_.push_back(previewBox_);

    tabBtns_[0] = makeBtn(-1, IDC_TAB_0, 14,  72, 110, 30, Tr(lang, StrId::TabAppearance));
    tabBtns_[1] = makeBtn(-1, IDC_TAB_1, 126, 72, 136, 30, Tr(lang, StrId::TabLocation));
    tabBtns_[2] = makeBtn(-1, IDC_TAB_2, 264, 72, 126, 30, Tr(lang, StrId::TabNotifications));
    tabBtns_[3] = makeBtn(-1, IDC_TAB_3, 392, 72, 106, 30, Tr(lang, StrId::TabFeatures));
    tabBtns_[4] = makeBtn(-1, IDC_TAB_4, 500, 72, 116, 30, Tr(lang, StrId::TabLanguageSystem));
    for (HWND b : tabBtns_) ShowWindow(b, SW_SHOW);

    footerInfo_ = makeLabel(-1, 14, 642, 480, 20, Tr(lang, StrId::FooterAutoSave), false, true);
    ShowWindow(footerInfo_, SW_SHOW);
    okBtn_ = makeBtn(-1, IDC_OK_BTN, 518, 636, 98, 30, Tr(lang, StrId::BtnOk));
    ShowWindow(okBtn_, SW_SHOW);

    // ==================== TAB 0: GÖRÜNÜM ====================
    makeLabel(0, 26, 121, 175, 22, Tr(lang, StrId::LblTaskbarStyle), true);
    styleCombo_ = makeCombo(0, IDC_STYLE_COMBO, 206, 118, 396, 220);

    for (int i = 0; i < 6; ++i) {
        int col = i % 3;
        int row = i / 3;
        makeBtn(0, static_cast<WORD>(IDC_PRESET_BASE + i), 26 + col * 194, 150 + row * 30, 186, 25, kQuickStyleLabels[i]);
    }

    makeLabel(0, 26, 218, 175, 22, Tr(lang, StrId::LblCustomTemplate));
    customEdit_ = makeEdit(0, IDC_CUSTOM_EDIT, 206, 215, 396, 23);

    int tx = 206;
    for (int i = 0; i < 7; ++i) {
        int tw = (i == 1 || i == 4) ? 66 : (i == 6 ? 36 : 50);
        makeBtn(0, static_cast<WORD>(IDC_TOKEN_BASE + i), tx, 243, tw, 22, kTokens[i]);
        tx += tw + 4;
    }

    makeLabel(0, 26, 279, 175, 22, Tr(lang, StrId::LblFlyoutDesign), true);
    flyoutCombo_ = makeCombo(0, IDC_FLYOUT_COMBO, 206, 276, 396, 160);

    makeLabel(0, 26, 340, 175, 22, Tr(lang, StrId::LblDockPos), true);
    dockCombo_ = makeCombo(0, IDC_DOCK_COMBO, 206, 337, 396, 150);

    makeLabel(0, 26, 372, 175, 22, Tr(lang, StrId::LblOffset));
    offsetSlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_TABSTOP | TBS_HORZ,
                                    S(206), S(370), S(296), S(24), hwnd_,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_OFFSET_SLIDER)), hInst, nullptr);
    SendMessageW(offsetSlider_, TBM_SETRANGEMIN, FALSE, -500);
    SendMessageW(offsetSlider_, TBM_SETRANGEMAX, FALSE, 500);
    SendMessageW(offsetSlider_, TBM_SETLINESIZE, 0, 5);
    SendMessageW(offsetSlider_, TBM_SETPAGESIZE, 0, 25);
    track(0, offsetSlider_);
    offsetLabel_ = makeLabel(0, 508, 373, 52, 20, L"0 px");
    makeBtn(0, IDC_OFFSET_RESET, 562, 371, 40, 23, L"0");

    showPillChk_ = makeChk(0, IDC_SHOW_PILL_CHK, 26, 402, 282, 22, Tr(lang, StrId::ChkShowPill));
    opaqueChk_ = makeChk(0, IDC_OPAQUE_CHK, 316, 402, 286, 22, Tr(lang, StrId::ChkOpaqueBg));
    showTrayChk_ = makeChk(0, IDC_SHOW_TRAY_CHK, 26, 428, 282, 22, Tr(lang, StrId::ChkShowTray));
    showBarChk_ = makeChk(0, IDC_SHOW_BAR_CHK, 316, 428, 286, 22, Tr(lang, StrId::ChkShowBar));
    showSecChk_ = makeChk(0, IDC_SHOW_SEC_CHK, 26, 454, 282, 22, Tr(lang, StrId::ChkShowSec));
    accentChk_ = makeChk(0, IDC_ACCENT_CHK, 316, 454, 286, 22, Tr(lang, StrId::ChkAccentSync));

    makeLabel(0, 26, 488, 175, 22, Tr(lang, StrId::LblTrayStyle));
    trayCombo_ = makeCombo(0, IDC_TRAY_COMBO, 206, 485, 396, 150);

    makeLabel(0, 26, 521, 175, 22, Tr(lang, StrId::LblFontSize));
    fontCombo_ = makeCombo(0, IDC_FONT_COMBO, 206, 518, 396, 130);

    makeLabel(0, 26, 554, 175, 22, Tr(lang, StrId::LblUrgentColor));
    urgentCombo_ = makeCombo(0, IDC_URGENT_COMBO, 206, 551, 396, 160);

    makeLabel(0, 26, 587, 175, 22, Tr(lang, StrId::LblTheme));
    themeCombo_ = makeCombo(0, IDC_THEME_COMBO, 206, 584, 396, 130);

    // ==================== TAB 1: KONUM & VAKİTLER ====================
    detectLocBtn_ = makeBtn(1, IDC_DETECT_LOC_BTN, 26, 120, 185, 28, Tr(lang, StrId::BtnDetectLocation));
    autoLocStatus_ = makeLabel(1, 220, 123, 382, 24, Tr(lang, StrId::LocDetectHint), false, true);
    autoMethodChk_ = makeChk(1, IDC_AUTO_METHOD_CHK, 26, 154, 576, 22, Tr(lang, StrId::ChkAutoMethod));

    makeLabel(1, 26, 184, 175, 22, Tr(lang, StrId::LblCitySearch), true);
    citySearchEdit_ = makeEdit(1, IDC_CITY_SEARCH_EDIT, 206, 181, 396, 24);

    cityList_ = CreateWindowExW(0, L"LISTBOX", L"",
                                WS_CHILD | WS_TABSTOP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                S(26), S(211), S(576), S(104),
                                hwnd_, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_CITY_LIST)), hInst, nullptr);
    applyCtrlTheme(cityList_, false);
    track(1, cityList_);

    customCoordChk_ = makeChk(1, IDC_CUSTOM_COORD_CHK, 26, 324, 156, 22, Tr(lang, StrId::ChkCustomCoord));
    makeLabel(1, 188, 326, 48, 20, Tr(lang, StrId::LblLat));
    latEdit_ = makeEdit(1, IDC_LAT_EDIT, 238, 323, 92, 24);
    makeLabel(1, 338, 326, 50, 20, Tr(lang, StrId::LblLon));
    lonEdit_ = makeEdit(1, IDC_LON_EDIT, 390, 323, 92, 24);
    applyCoordBtn_ = makeBtn(1, IDC_APPLY_COORD_BTN, 492, 322, 110, 26, Tr(lang, StrId::BtnApplyCoord));

    makeLabel(1, 26, 378, 175, 22, Tr(lang, StrId::LblCalcMethod), true);
    methodCombo_ = makeCombo(1, IDC_METHOD_COMBO, 206, 375, 396, 280);

    makeLabel(1, 26, 412, 175, 22, Tr(lang, StrId::LblAsrMethod));
    asrCombo_ = makeCombo(1, IDC_ASR_COMBO, 206, 409, 396, 120);

    onlineSyncChk_ = makeChk(1, IDC_ONLINE_SYNC_CHK, 26, 444, 576, 22, Tr(lang, StrId::ChkOnlineSync));
    syncBtn_ = makeBtn(1, IDC_SYNC_NOW_BTN, 26, 472, 170, 27, Tr(lang, StrId::BtnSyncNow));
    openImsakiyeBtn_ = makeBtn(1, IDC_OPEN_IMSAKIYE_BTN, 204, 472, 170, 27, Tr(lang, StrId::BtnOpenImsakiye));
    syncStatus_ = makeLabel(1, 26, 504, 576, 20, L"", false, true);

    makeLabel(1, 26, 530, 340, 20, Tr(lang, StrId::LblOffsets), true);
    makeBtn(1, IDC_APPLY_OFFSETS_BTN, 476, 527, 126, 24, Tr(lang, StrId::BtnApplyOffsets));
    for (int i = 0; i < 6; ++i) {
        int col = i % 3;
        int row = i / 3;
        int bx = 26 + col * 195;
        int by = 556 + row * 31;
        makeLabel(1, bx, by + 3, 74, 20, GetEffectivePrayerName(static_cast<PrayerKind>(i), settings_).c_str());
        offsetEdits_[i] = makeEdit(1, static_cast<WORD>(IDC_OFFSET_EDIT_BASE + i), bx + 80, by, 72, 24);
    }

    // ==================== TAB 2: BİLDİRİMLER ====================
    // Card 1: per-prayer alerts (one row per prayer, roomy 30px rhythm)
    makeLabel(2, 26, 118, 400, 22, Tr(lang, StrId::SectionPrayerAlerts), true);
    makeLabel(2, 26, 144, 118, 18, Tr(lang, StrId::ColPrayer), false, true);
    makeLabel(2, 150, 144, 104, 18, Tr(lang, StrId::ColAtTime), false, true);
    makeLabel(2, 262, 144, 200, 18, Tr(lang, StrId::ColPreRemind), false, true);
    makeLabel(2, 478, 144, 124, 18, Tr(lang, StrId::ColSoundEzan), false, true);

    for (int i = 0; i < 6; ++i) {
        int ry = 166 + i * 30;
        makeLabel(2, 26, ry + 3, 118, 20, GetEffectivePrayerName(static_cast<PrayerKind>(i), settings_).c_str(), true);
        alertAtTimeChk_[i] = makeChk(2, static_cast<WORD>(IDC_ALERT_TIME_BASE + i), 150, ry + 1, 104, 22, Tr(lang, StrId::ChkNotify));
        alertPreCombo_[i] = makeCombo(2, static_cast<WORD>(IDC_ALERT_PRE_BASE + i), 262, ry - 1, 200, 200);
        alertSoundChk_[i] = makeChk(2, static_cast<WORD>(IDC_ALERT_SOUND_BASE + i), 478, ry + 1, 124, 22, Tr(lang, StrId::ChkPlaySound));
    }

    // Card 2: sounds
    makeLabel(2, 26, 368, 400, 22, Tr(lang, StrId::SectionSounds), true);
    makeLabel(2, 26, 397, 175, 22, Tr(lang, StrId::LblEntrySound));
    entrySoundCombo_ = makeCombo(2, IDC_ENTRY_SOUND_COMBO, 206, 394, 396, 180);
    makeLabel(2, 26, 429, 175, 22, Tr(lang, StrId::LblFajrSound));
    fajrSoundCombo_ = makeCombo(2, IDC_FAJR_SOUND_COMBO, 206, 426, 396, 200);
    makeLabel(2, 26, 461, 175, 22, Tr(lang, StrId::LblPreSound));
    preSoundCombo_ = makeCombo(2, IDC_PRE_SOUND_COMBO, 206, 458, 396, 180);

    makeLabel(2, 26, 493, 175, 22, Tr(lang, StrId::LblCustomSound));
    customSoundEdit_ = makeEdit(2, IDC_CUSTOM_SOUND_EDIT, 206, 490, 302, 24, true);
    browseSoundBtn_ = makeBtn(2, IDC_BROWSE_SOUND_BTN, 514, 489, 88, 26, Tr(lang, StrId::BtnBrowse));

    makeLabel(2, 26, 524, 175, 22, Tr(lang, StrId::LblVolume));
    volumeSlider_ = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_TABSTOP | TBS_HORZ,
                                    S(206), S(522), S(338), S(26), hwnd_,
                                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_VOLUME_SLIDER)), hInst, nullptr);
    SendMessageW(volumeSlider_, TBM_SETRANGEMIN, FALSE, 0);
    SendMessageW(volumeSlider_, TBM_SETRANGEMAX, FALSE, 100);
    track(2, volumeSlider_);
    volumeLabel_ = makeLabel(2, 550, 525, 52, 20, L"%80");

    makeBtn(2, IDC_TEST_ENTRY_BTN, 26, 556, 138, 28, Tr(lang, StrId::BtnTestEntry));
    makeBtn(2, IDC_TEST_PRE_BTN, 170, 556, 138, 28, Tr(lang, StrId::BtnTestPre));
    makeBtn(2, IDC_STOP_SOUND_BTN, 314, 556, 118, 28, Tr(lang, StrId::BtnStopSound));
    makeBtn(2, IDC_TEST_NOTIF_BTN, 438, 556, 164, 28, Tr(lang, StrId::BtnTestNotif));

    // ==================== TAB 3: ÖZELLİKLER ====================
    // Every optional feature in one place: a toggle plus a one-line explanation.
    auto feature = [&](WORD id, int y, StrId title, StrId desc, bool hasDesc) -> HWND {
        HWND c = makeChk(3, id, 26, y, 576, 22, Tr(lang, title));
        if (hasDesc) makeLabel(3, 54, y + 22, 548, 18, Tr(lang, desc), false, true);
        return c;
    };
    makeLabel(3, 26, 118, 400, 22, Tr(lang, StrId::SectionReminders), true);
    fridayChk_ = feature(IDC_FRIDAY_CHK, 144, StrId::ChkFridayReminder, StrId::ChkFridayReminder, false);
    holidayChk_ = feature(IDC_HOLIDAY_CHK, 174, StrId::ChkHolidays, StrId::DescHolidays, true);
    travelChk_ = feature(IDC_TRAVEL_CHK, 218, StrId::ChkTravelMode, StrId::DescTravelMode, true);
    toastChk_ = feature(IDC_TOAST_CHK, 262, StrId::ChkModernToast, StrId::DescModernToast, true);
    quietFullscreenChk_ = feature(IDC_QUIET_FULLSCREEN_CHK, 306, StrId::ChkQuietFullscreen, StrId::ChkQuietFullscreen, false);

    makeLabel(3, 26, 358, 400, 22, Tr(lang, StrId::SectionDisplayTracking), true);
    kerahatChk_ = feature(IDC_KERAHAT_CHK, 384, StrId::ChkKerahatIndicator, StrId::ChkKerahatIndicator, false);
    ramadanChk_ = feature(IDC_RAMADAN_CHK, 414, StrId::ChkRamadanMode, StrId::ChkRamadanMode, false);
    extraTimesChk_ = feature(IDC_EXTRA_TIMES_CHK, 444, StrId::ChkExtraTimes, StrId::DescExtraTimes, true);
    trackingChk_ = feature(IDC_TRACKING_CHK, 488, StrId::ChkTracking, StrId::DescTracking, true);
    hotkeyChk_ = makeChk(3, IDC_HOTKEY_CHK, 26, 532, 380, 22, Tr(lang, StrId::ChkHotkey));
    hotkeyBtn_ = makeBtn(3, IDC_HOTKEY_BTN, 412, 527, 190, 26, L"");
    hotkeyDesc_ = makeLabel(3, 54, 556, 548, 18, Tr(lang, StrId::DescHotkey), false, true);
    capturingHotkey_ = false;
    SetWindowSubclass(hotkeyBtn_, HotkeyBtnProc, 1, reinterpret_cast<DWORD_PTR>(this));

    // ==================== TAB 4: DİL & SİSTEM ====================
    makeLabel(4, 26, 128, 175, 22, Tr(lang, StrId::LblAppLanguage), true);
    langCombo_ = makeCombo(4, IDC_LANG_COMBO, 206, 125, 396, 200);

    makeLabel(4, 26, 182, 576, 20, Tr(lang, StrId::SectionCustomNames), true);
    makeLabel(4, 26, 207, 130, 18, Tr(lang, StrId::ColDefaultName), false, true);
    makeLabel(4, 176, 207, 220, 18, Tr(lang, StrId::ColCustomName), false, true);
    makeLabel(4, 416, 207, 120, 18, Tr(lang, StrId::ColShortCode), false, true);

    for (int i = 0; i < 6; ++i) {
        int ry = 228 + i * 30;
        makeLabel(4, 26, ry + 3, 140, 20, GetDefaultPrayerName(static_cast<PrayerKind>(i), lang), true);
        customNameEdits_[i] = makeEdit(4, static_cast<WORD>(IDC_CUSTOM_NAME_BASE + i), 176, ry, 226, 24);
        customShortEdits_[i] = makeEdit(4, static_cast<WORD>(IDC_CUSTOM_SHORT_BASE + i), 416, ry, 92, 24);
    }

    makeBtn(4, IDC_APPLY_NAMES_BTN, 26, 410, 170, 27, Tr(lang, StrId::BtnApplyNames));
    makeBtn(4, IDC_RESET_NAMES_BTN, 204, 410, 170, 27, Tr(lang, StrId::BtnResetNames));

    makeLabel(4, 26, 458, 576, 20, Tr(lang, StrId::SectionSystem), true);
    startupChk_ = makeChk(4, IDC_STARTUP_CHK, 26, 480, 576, 21, Tr(lang, StrId::ChkStartup));
    autoMemChk_ = makeChk(4, IDC_AUTO_MEM_CHK, 26, 504, 576, 21, Tr(lang, StrId::ChkAutoMem));
    telemetryLabel_ = makeLabel(4, 26, 528, 576, 18, L"...", false, true);

    makeBtn(4, IDC_EXPORT_BTN, 26, 550, 175, 26, Tr(lang, StrId::BtnExportSettings));
    makeBtn(4, IDC_IMPORT_BTN, 209, 550, 175, 26, Tr(lang, StrId::BtnImportSettings));
    sysStatusLabel_ = makeLabel(4, 394, 554, 208, 20, L"", false, true);

    licenseLabel_ = makeLabel(4, 26, 586, 416, 20, L"", true, true);
    makeBtn(4, IDC_OPEN_PRO_BTN, 448, 583, 154, 27, Tr(lang, StrId::BtnOpenProWindow));
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

    SetWindowTextW(customEdit_, settings_.customTemplate.c_str());

    std::vector<std::pair<int, std::wstring>> flyoutItems;
    for (int i = 0; i <= 3; ++i) {
        flyoutItems.emplace_back(i, GetFlyoutDesignFriendlyName(static_cast<FlyoutDesignStyle>(i), lang));
    }
    fillCombo(flyoutCombo_, flyoutItems);
    SetComboSelectionByData(flyoutCombo_, static_cast<int>(settings_.flyoutDesign));

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

    // Tab 2
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

    // Tab 3
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
    RefreshPreviewAndMemory();
    RefreshLicenseStatus();
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

void SettingsWindow::SwitchTab(int tabIdx) {
    activeTab_ = std::clamp(tabIdx, 0, 4);
    for (int t = 0; t < 5; ++t) {
        int showCmd = (t == activeTab_) ? SW_SHOW : SW_HIDE;
        for (HWND h : tabControls_[t]) {
            ShowWindow(h, showCmd);
        }
        InvalidateRect(tabBtns_[t], nullptr, FALSE);
        AnnotateAccessible(tabBtns_[t], ROLE_SYSTEM_PAGETAB,
                           STATE_SYSTEM_FOCUSABLE | (t == activeTab_ ? STATE_SYSTEM_SELECTED : 0));
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
    if (activeTab_ == 4) {
        RefreshPreviewAndMemory();
    }
}

void SettingsWindow::SaveAndNotify() {
    if (isLoading_) return;
    settingsStore_.Save(settings_);
    if (onSettingsSaved_) onSettingsSaved_();
    RefreshPreviewAndMemory();
    UpdateHotkeyUi();  // registration result is known right after the host re-applies settings
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

// While capturing, the button swallows keystrokes and turns the first non-modifier key into the shortcut.
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
                return 0;  // wait for the actual key
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
            self->settings_.globalHotkey = true;  // choosing a shortcut turns the feature on
            self->SetFluentCheck(IDC_HOTKEY_CHK, self->hotkeyChk_, true);
            self->SaveAndNotify();
            return 0;
        }
    }
    return DefSubclassProc(h, msg, wp, lp);
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
            uint32_t bg = isDark_ ? 0xFF202020u : 0xFFF3F3F3u;
            uint32_t cardBg = isDark_ ? 0xFF2B2B2Eu : 0xFFFFFFFFu;
            uint32_t cardBdr = isDark_ ? 0x1EFFFFFFu : 0x18000000u;

            gdip::GdipGraphicsClear(g, bg);
            float s = static_cast<float>(scale_);
            for (const auto& box : GetCardsForTab(activeTab_)) {
                gdip::FillRoundedRect(g, box.x * s, box.y * s, box.w * s, box.h * s, 8.0f * s, cardBg);
                gdip::StrokeRoundedRect(g, box.x * s, box.y * s, box.w * s, box.h * s, 8.0f * s, cardBdr, 1.0f);
            }
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gdiBmp);
    }

    BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(hBmp);
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
    bool accentBtn = IsAccentButton(id);
    bool inCard = IsControlInCard(id);

    uint32_t accent = GetWindowsAccentColorArgb(isDark_, settings_.useSystemAccentColor);
    uint32_t parentBg = inCard ? (isDark_ ? 0xFF2B2B2Eu : 0xFFFFFFFFu) : (isDark_ ? 0xFF202020u : 0xFFF3F3F3u);

    uint32_t fill = 0;
    uint32_t border = 0;
    uint32_t fg = 0;
    if (accentBtn) {
        fill = disabled ? (isDark_ ? 0xFF333338u : 0xFFE0E2E6u) : (pressed ? (0xCC000000u | (accent & 0x00FFFFFFu)) : accent);
        border = accent;
        fg = disabled ? (isDark_ ? 0xFF7D828Cu : 0xFF999FA8u) : (isDark_ ? 0xFF101414u : 0xFFFFFFFFu);
    } else {
        fill = pressed
            ? (isDark_ ? 0xFF3F4046u : 0xFFE3E6ECu)
            : (isDark_ ? 0xFF34353Au : 0xFFFAFAFCu);
        border = isDark_ ? 0x26FFFFFFu : 0x22000000u;
        fg = disabled ? (isDark_ ? 0xFF7D828Cu : 0xFF999FA8u) : (isDark_ ? 0xFFF2F4F8u : 0xFF1A1A1Au);
    }

    int len = GetWindowTextLengthW(dis->hwndItem);
    std::wstring label;
    if (len > 0) {
        label.resize(static_cast<size_t>(len) + 1);
        GetWindowTextW(dis->hwndItem, label.data(), len + 1);
        label.resize(static_cast<size_t>(len));
    }

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
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAliasGridFit);
            gdip::GdipGraphicsClear(g, parentBg);

            float s = static_cast<float>(scale_);
            gdip::FillRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 5.0f * s, fill);
            gdip::StrokeRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 5.0f * s, border, 1.0f);

            void* fam = gdip::CreateBestUiFontFamily(false);
            gdip::DrawTextBox(g, label, fam, 12.0f * s, accentBtn, fg,
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
    uint32_t accent = GetWindowsAccentColorArgb(isDark_, settings_.useSystemAccentColor);
    uint32_t parentBg = inCard ? (isDark_ ? 0xFF2B2B2Eu : 0xFFFFFFFFu) : (isDark_ ? 0xFF202020u : 0xFFF3F3F3u);

    int len = GetWindowTextLengthW(dis->hwndItem);
    std::wstring label;
    if (len > 0) {
        label.resize(static_cast<size_t>(len) + 1);
        GetWindowTextW(dis->hwndItem, label.data(), len + 1);
        label.resize(static_cast<size_t>(len));
    }

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
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAliasGridFit);
            gdip::GdipGraphicsClear(g, parentBg);

            float s = static_cast<float>(scale_);
            float boxSz = 16.0f * s;
            float by = (static_cast<float>(h) - boxSz) * 0.5f;

            if (checked) {
                gdip::FillRoundedRect(g, 1.0f * s, by, boxSz, boxSz, 4.0f * s, accent);
                void* pen = nullptr;
                uint32_t tickCol = isDark_ ? 0xFF101414u : 0xFFFFFFFFu;
                gdip::GdipCreatePen1(tickCol, 2.0f * s, gdip::kUnitPixel, &pen);
                if (pen) {
                    gdip::GdipDrawLine(g, pen, 4.5f * s, by + 8.2f * s, 7.2f * s, by + 11.2f * s);
                    gdip::GdipDrawLine(g, pen, 7.2f * s, by + 11.2f * s, 13.0f * s, by + 4.8f * s);
                    gdip::GdipDeletePen(pen);
                }
            } else {
                uint32_t boxFill = isDark_ ? 0xFF25262Au : 0xFFF7F8FAu;
                uint32_t boxBdr = isDark_ ? 0x55FFFFFFu : 0x55000000u;
                gdip::FillRoundedRect(g, 1.0f * s, by, boxSz, boxSz, 4.0f * s, boxFill);
                gdip::StrokeRoundedRect(g, 1.0f * s, by, boxSz, boxSz, 4.0f * s, boxBdr, 1.1f);
            }

            void* fam = gdip::CreateBestUiFontFamily(false);
            uint32_t fg = isDark_ ? 0xFFF0F2F6u : 0xFF1A1A1Au;
            gdip::DrawTextBox(g, label, fam, 12.2f * s, false, fg,
                              boxSz + 8.0f * s, 0.0f, static_cast<float>(w) - boxSz - 10.0f * s, static_cast<float>(h),
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
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

void SettingsWindow::PaintTabHeader(HDC hdc, int tabIdx, const RECT& rc) {
    gdip::EnsureInitialized();
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    bool active = (tabIdx == activeTab_);
    uint32_t bg = isDark_ ? (active ? 0xFF2B2B2Eu : 0xFF202020u) : (active ? 0xFFFFFFFFu : 0xFFEBECEFu);
    uint32_t fg = isDark_ ? (active ? 0xFFFFFFFFu : 0xFFA4AAB8u) : (active ? 0xFF1A1A1Au : 0xFF5D6472u);
    uint32_t accent = GetWindowsAccentColorArgb(isDark_, settings_.useSystemAccentColor);
    uint32_t border = isDark_ ? 0x22FFFFFFu : 0x1E000000u;

    void* g = nullptr;
    if (gdip::GdipCreateFromHDC(hdc, &g) == 0 && g) {
        gdip::GdipSetSmoothingMode(g, gdip::kSmoothingModeAntiAlias);
        gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAliasGridFit);
        gdip::GdipGraphicsClear(g, isDark_ ? 0xFF202020u : 0xFFF3F3F3u);

        float s = static_cast<float>(scale_);
        gdip::FillRoundedRect(g, 1.0f, 1.0f, static_cast<float>(w) - 2.0f, static_cast<float>(h) - 2.0f, 6.0f * s, bg);
        if (active) {
            gdip::StrokeRoundedRect(g, 1.0f, 1.0f, static_cast<float>(w) - 2.0f, static_cast<float>(h) - 2.0f, 6.0f * s, border, 1.0f);
            gdip::FillRoundedRect(g, 14.0f * s, static_cast<float>(h) - 3.5f * s, static_cast<float>(w) - 28.0f * s, 2.5f * s, 1.5f * s, accent);
        }

        const StrId titleIds[5] = {StrId::TabAppearance, StrId::TabLocation, StrId::TabNotifications, StrId::TabFeatures, StrId::TabLanguageSystem};
        const wchar_t* title = Tr(settings_.language, titleIds[std::clamp(tabIdx, 0, 4)]);
        void* family = gdip::CreateBestUiFontFamily(false);
        gdip::DrawTextBox(g, title, family, 12.2f * s, active, fg, 0.0f, 0.0f,
                          static_cast<float>(w), static_cast<float>(h) - 2.0f * s,
                          gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
        if (family) gdip::GdipDeleteFontFamily(family);
        gdip::GdipDeleteGraphics(g);
    }
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
            gdip::GdipSetTextRenderingHint(g, gdip::kTextRenderingHintAntiAliasGridFit);

            float s = static_cast<float>(scale_);
            float w = static_cast<float>(width);
            float h = static_cast<float>(height);
            bool tbLight = IsTaskbarLight(settings_.themeMode);
            bool useOpaque = settings_.opaqueTaskbarBackground || settings_.dockPosition == TaskbarDockPosition::FarRight;

            gdip::GdipGraphicsClear(g, isDark_ ? 0xFF202020u : 0xFFF3F3F3u);
            uint32_t tbBg = tbLight ? 0xFFEAEDF2u : 0xFF1A1B1Fu;
            gdip::FillRoundedRect(g, 0.5f, 0.5f, w - 1.0f, h - 1.0f, 8.0f * s, tbBg);

            LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
            PrayerCountdownState state = scheduleService_.ComputeState(now, settings_);
            FormattedTaskbarDisplay disp = FormatTaskbar(state, settings_);

            void* family = gdip::CreateBestUiFontFamily(false);
            uint32_t mockTxt = tbLight ? 0xAA000000u : 0xAAFFFFFFu;
            uint32_t pillBg = useOpaque
                ? (tbLight ? 0xFFF3F3F3u : 0xFF1F2024u)
                : (tbLight ? 0x18000000u : 0x1AFFFFFFu);
            uint32_t pillPrim = tbLight ? 0xE6000000u : 0xFFFFFFFFu;
            uint32_t pillSec = tbLight ? 0xA8000000u : 0xB8FFFFFFu;
            uint32_t accent = disp.isUrgent
                ? 0xFFFFCA62u
                : (settings_.useSystemAccentColor ? GetWindowsAccentColorArgb(!tbLight, true) : GetPrayerAccentArgb(disp.nextPrayer));

            gdip::DrawTextBox(g, Tr(settings_.language, StrId::LivePreviewLabel), family, 11.0f * s, false, mockTxt,
                              12.0f * s, 0.0f, 220.0f * s, h, gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            float trayRightW = 148.0f * s;
            gdip::DrawTextBox(g, L"^   Wi-Fi  %71   15:02", family, 11.0f * s, false, mockTxt,
                              w - trayRightW - 10.0f * s, 0.0f, trayRightW, h,
                              gdip::kStringAlignmentFar, gdip::kStringAlignmentCenter);

            float pillW = std::clamp(static_cast<float>(MeasureDesiredPillWidth(disp, settings_, 1.0)) * s, 54.0f * s, 195.0f * s);
            float pillH = h - 12.0f * s;
            float pillX = w - trayRightW - 18.0f * s - pillW;
            float pillY = 6.0f * s;
            gdip::FillRoundedRect(g, pillX, pillY, pillW, pillH, 4.0f * s, pillBg);
            gdip::StrokeRoundedRect(g, pillX, pillY, pillW, pillH, 4.0f * s,
                                    tbLight ? 0x22000000u : 0x28FFFFFFu, 1.0f);

            if (disp.hasLine2) {
                gdip::DrawTextBox(g, disp.line1, family, 10.5f * s, false, pillSec,
                                  pillX + 4.0f * s, pillY + 2.0f * s, pillW - 8.0f * s, (pillH - 6.0f * s) * 0.48f,
                                  gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
                gdip::DrawTextBox(g, disp.line2, family, 12.0f * s, true, pillPrim,
                                  pillX + 4.0f * s, pillY + (pillH - 6.0f * s) * 0.45f, pillW - 8.0f * s, (pillH - 6.0f * s) * 0.50f,
                                  gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
            } else {
                std::wstring txt = disp.showPhaseIcon ? (L"◐ " + disp.line1) : (disp.showDot ? (L"● " + disp.line1) : disp.line1);
                gdip::DrawTextBox(g, txt, family, 12.0f * s, true, pillPrim,
                                  pillX + 4.0f * s, pillY, pillW - 8.0f * s, pillH - 4.0f * s,
                                  gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
            }

            if (settings_.showProgressBar) {
                float bw = pillW - 14.0f * s;
                gdip::FillRoundedRect(g, pillX + 7.0f * s, pillY + pillH - 4.0f * s, bw, 2.0f * s, 1.0f,
                                      tbLight ? 0x25000000u : 0x30FFFFFFu);
                gdip::FillRoundedRect(g, pillX + 7.0f * s, pillY + pillH - 4.0f * s,
                                      std::max(6.0f * s, bw * static_cast<float>(disp.progress)), 2.0f * s, 1.0f, accent);
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

    std::thread([target]() {
        auto detected = ScheduleService::DetectLocationByIp();
        auto* heapPtr = detected ? new DetectedLocationResult(*detected) : nullptr;
        if (!PostMessageW(target, kMsgLocDetected, 0, reinterpret_cast<LPARAM>(heapPtr))) {
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

    if (id >= IDC_TAB_0 && id <= IDC_TAB_4) {
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
            if (onRequestUpgrade_) onRequestUpgrade_(true);
            return;
        }
        settings_.displayStyle = chosen;
        SaveAndNotify();
        return;
    }
    if (id >= IDC_PRESET_BASE && id < IDC_PRESET_BASE + 6 && code == BN_CLICKED) {
        auto chosen = kQuickStyles[id - IDC_PRESET_BASE];
        if (LicenseManager::IsFeatureProOnly(chosen) && !licenseManager_.IsProActive()) {
            if (onRequestUpgrade_) onRequestUpgrade_(true);
            return;
        }
        settings_.displayStyle = chosen;
        isLoading_ = true;
        SetComboSelectionByData(styleCombo_, static_cast<int>(settings_.displayStyle));
        isLoading_ = false;
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
            if (onRequestUpgrade_) onRequestUpgrade_(true);
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
        SaveAndNotify();
        return;
    }
    if (id == IDC_FLYOUT_COMBO && code == CBN_SELCHANGE) {
        auto chosen = static_cast<FlyoutDesignStyle>(GetComboSelectedData(flyoutCombo_, 0));
        if (LicenseManager::IsFeatureProOnly(chosen) && !licenseManager_.IsProActive()) {
            isLoading_ = true;
            SetComboSelectionByData(flyoutCombo_, static_cast<int>(settings_.flyoutDesign));
            isLoading_ = false;
            if (onRequestUpgrade_) onRequestUpgrade_(true);
            return;
        }
        settings_.flyoutDesign = chosen;
        SaveAndNotify();
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
         id == IDC_SHOW_BAR_CHK || id == IDC_SHOW_SEC_CHK || id == IDC_ACCENT_CHK) && code == BN_CLICKED) {
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
        SaveAndNotify();
        if (id == IDC_ACCENT_CHK) {
            PostMessageW(hwnd_, kMsgRebuildUi, 0, 0);
        }
        return;
    }
    if ((id == IDC_TRAY_COMBO || id == IDC_FONT_COMBO || id == IDC_URGENT_COMBO || id == IDC_THEME_COMBO) && code == CBN_SELCHANGE) {
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
    if (id == IDC_AUTO_METHOD_CHK && code == BN_CLICKED) {
        bool nextVal = !GetFluentCheck(IDC_AUTO_METHOD_CHK);
        SetFluentCheck(IDC_AUTO_METHOD_CHK, ctrl, nextVal);
        settings_.autoSelectMethodByCountry = nextVal;
        SaveAndNotify();
        return;
    }
    if (id == IDC_CITY_SEARCH_EDIT && code == EN_CHANGE) {
        RefreshCityList(GetEditText(citySearchEdit_));
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

            isLoading_ = true;
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
        settings_.useCustomCoordinates = nextVal;
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
            settings_.latitude = std::clamp(lat, -70.0, 70.0);
            settings_.longitude = std::clamp(lon, -180.0, 180.0);
            settings_.useCustomCoordinates = true;
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

    // Tab 2
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
    // Features tab: simple on/off toggles
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
            if (onRequestUpgrade_) onRequestUpgrade_(true);
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
        if (onTestNotification_) {
            onTestNotification_(
                Tr(settings_.language, StrId::TestNotifTitle),
                Tr(settings_.language, StrId::TestNotifBody),
                playAudio);
        }
        if (playAudio) {
            AudioPlayer::Play(settings_.preNotifySound, settings_.customSoundPath, settings_.soundVolumePercent);
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

    // Tab 3 (Dil & Sistem)
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
            if (onRequestUpgrade_) onRequestUpgrade_(true);
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
            if (onRequestUpgrade_) onRequestUpgrade_(true);
            return;
        }
        wchar_t fileBuf[MAX_PATH] = L"vakitcubugu_settings.json";
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
            if (onRequestUpgrade_) onRequestUpgrade_(true);
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
        if (onRequestUpgrade_) onRequestUpgrade_(false);
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
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, isDark_ ? (inCard ? RGB(43, 43, 46) : RGB(32, 32, 32))
                                    : (inCard ? RGB(255, 255, 255) : RGB(243, 243, 243)));
            SetTextColor(hdc, isDark_ ? RGB(240, 242, 246) : RGB(26, 26, 26));
            return reinterpret_cast<LRESULT>(inCard ? cardBrush_ : bgBrush_);
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkColor(hdc, isDark_ ? RGB(50, 50, 56) : RGB(255, 255, 255));
            SetTextColor(hdc, isDark_ ? RGB(242, 244, 248) : RGB(20, 22, 26));
            return reinterpret_cast<LRESULT>(editBrush_);
        }

        case WM_DRAWITEM: {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (!dis) break;
            if (dis->CtlID == IDC_PREVIEW_BOX) {
                PaintTaskbarPreview(dis->hDC, dis->rcItem.right - dis->rcItem.left, dis->rcItem.bottom - dis->rcItem.top);
                return TRUE;
            }
            if (dis->CtlID >= IDC_TAB_0 && dis->CtlID <= IDC_TAB_4) {
                PaintTabHeader(dis->hDC, dis->CtlID - IDC_TAB_0, dis->rcItem);
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

        case kMsgLocDetected: {
            auto* detected = reinterpret_cast<DetectedLocationResult*>(lParam);
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
            settings_.useCustomCoordinates = (detected->cityId == L"custom");
            if (settings_.autoSelectMethodByCountry) {
                settings_.calculationMethod = detected->recommendedMethod;
                SetComboSelectionByData(methodCombo_, static_cast<int>(settings_.calculationMethod));
            }
            SetFluentCheck(IDC_CUSTOM_COORD_CHK, customCoordChk_, settings_.useCustomCoordinates);
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

            std::wstring statusMsg = detected->cityName + L", " + detected->countryName + L" (" + detected->sourceLabel + L") · " +
                                     GetMethodFriendlyName(settings_.calculationMethod);
            SetWindowTextW(autoLocStatus_, statusMsg.c_str());
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

        case WM_CLOSE:
            CloseNow();
            return 0;

        case WM_DESTROY:
            hwnd_ = nullptr;
            allCreatedControls_.clear();
            for (auto& vec : tabControls_) vec.clear();
            cardStaticLabels_.clear();
            checkStates_.clear();
            checkInCard_.clear();
            return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace vakit
