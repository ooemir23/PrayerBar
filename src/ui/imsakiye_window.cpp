#include "imsakiye_window.h"
#include "gdiplus_flat.h"
#include "../core/i18n.h"
#include "../core/prayer_calc.h"
#include "../core/utf_util.h"
#include "../platform/theme_utils.h"

#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>
#include <thread>

namespace vakit {

namespace {

constexpr wchar_t kImsakiyeClassName[] = L"VakitCubuguImsakiyeWindow";
constexpr int kDipWidth = 740;
constexpr int kDipHeight = 560;
constexpr int kHeaderDipY = 86;
constexpr int kFooterDipH = 26;
constexpr int kRowDipH = 27;

uint32_t WithAlpha(uint8_t alpha, uint32_t argb) {
    return (static_cast<uint32_t>(alpha) << 24) | (argb & 0x00FFFFFFu);
}

std::wstring HtmlEscape(std::wstring_view s) {
    std::wstring out;
    out.reserve(s.size() + 16);
    for (wchar_t ch : s) {
        switch (ch) {
            case L'&': out += L"&amp;"; break;
            case L'<': out += L"&lt;"; break;
            case L'>': out += L"&gt;"; break;
            case L'"': out += L"&quot;"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

} // namespace

ImsakiyeWindow::ImsakiyeWindow(
    SettingsStore& settingsStore,
    ScheduleService& scheduleService,
    LicenseManager& licenseManager,
    std::function<void()> onRequestUpgrade)
    : settingsStore_(settingsStore),
      scheduleService_(scheduleService),
      licenseManager_(licenseManager),
      onRequestUpgrade_(std::move(onRequestUpgrade)) {

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kImsakiyeClassName;
    RegisterClassExW(&wc);
}

ImsakiyeWindow::~ImsakiyeWindow() {
    CloseNow();
}

void ImsakiyeWindow::OpenOrFocus() {
    settings_ = settingsStore_.Load();

    if (hwnd_) {
        if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
        RefreshIfOpen();
        return;
    }

    LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
    viewYear_ = now.date.year;
    viewMonth_ = now.date.month;
    ramadanMode_ = false;
    statusText_.clear();
    scrollY_ = 0;

    POINT pt{};
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);

    HWND tb = FindWindowW(L"Shell_TrayWnd", nullptr);
    UINT dpi = tb ? GetDpiForWindowCompat(tb) : 96;
    scale_ = dpi >= 96 ? static_cast<double>(dpi) / 96.0 : 1.0;

    int winW = static_cast<int>(std::llround(kDipWidth * scale_));
    int winH = static_cast<int>(std::llround(kDipHeight * scale_));
    int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - winW) / 2;
    int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - winH) / 2;

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VSCROLL;
    hwnd_ = CreateWindowExW(
        0,
        kImsakiyeClassName,
        Tr(settings_.language, StrId::ImsakiyeWindowTitle),
        style,
        x, y, winW, winH,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return;

    bool isLight = IsAppLight(settings_.themeMode);
    ApplyWindowDwmStyle(hwnd_, !isLight, 2 /*DWMWCP_ROUND*/, 2 /*DWMSBT_MAINWINDOW*/);

    LoadRows();
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    UpdateScrollBar(rc.right - rc.left, rc.bottom - rc.top);

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void ImsakiyeWindow::CloseNow() {
    if (!hwnd_) return;
    HWND h = hwnd_;
    hwnd_ = nullptr;
    DestroyWindow(h);
    rows_.clear();
    rows_.shrink_to_fit();
    if (settings_.autoOptimizeMemory) {
        TrimWorkingSet();
    }
}

void ImsakiyeWindow::RefreshIfOpen() {
    if (!hwnd_) return;
    settings_ = settingsStore_.Load();
    SetWindowTextW(hwnd_, Tr(settings_.language, StrId::ImsakiyeWindowTitle));
    bool isLight = IsAppLight(settings_.themeMode);
    ApplyWindowDwmStyle(hwnd_, !isLight, 2, 2);
    LoadRows();
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    UpdateScrollBar(rc.right - rc.left, rc.bottom - rc.top);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void ImsakiyeWindow::LoadRows() {
    rows_.clear();
    if (ramadanMode_) {
        rows_.reserve(30);
        for (int i = 0; i < 30; ++i) {
            DateYMD d = ramadanStart_.AddDays(i);
            rows_.push_back(scheduleService_.GetDaySchedule(d, settings_));
        }
    } else {
        rows_ = scheduleService_.GetMonthSchedule(viewYear_, viewMonth_, settings_, settings_.useOnlineSync);
    }

    // Bugünkü satırı görünür kıl (ilk açılışta veya ay değişince)
    if (hwnd_) {
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        ClampScroll(rc.bottom - rc.top);
        UpdateScrollBar(rc.right - rc.left, rc.bottom - rc.top);
    }
}

void ImsakiyeWindow::StartBackgroundSync() {
    if (syncBusy_) return;
    syncBusy_ = true;
    statusText_ = Tr(settings_.language, StrId::SyncingText);
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);

    HWND target = hwnd_;
    AppSettings copy = settings_;
    ScheduleService* svc = &scheduleService_;
    bool ramadan = ramadanMode_;
    int y = viewYear_;
    int m = viewMonth_;
    DateYMD rStart = ramadanStart_;

    std::thread([target, svc, copy, ramadan, y, m, rStart]() {
        bool ok = false;
        if (ramadan) {
            std::set<std::pair<int, int>> months;
            for (int i = 0; i < 30; ++i) {
                DateYMD d = rStart.AddDays(i);
                months.insert({d.year, d.month});
            }
            for (auto [my, mm] : months) {
                ok = svc->ForceRefreshMonth({my, mm, 1}, copy) || ok;
            }
        } else {
            ok = svc->ForceRefreshMonth({y, m, 1}, copy);
        }
        PostMessageW(target, kMsgMonthSynced, ok ? 1 : 0, 0);
    }).detach();
}

void ImsakiyeWindow::ExportCsv() {
    if (rows_.empty()) return;
    if (!licenseManager_.IsProActive()) {
        if (onRequestUpgrade_) onRequestUpgrade_();
        return;
    }
    wchar_t fileBuf[MAX_PATH]{};
    wchar_t defName[128];
    if (ramadanMode_) {
        HijriDateYMD h = GregorianToHijri(ramadanStart_);
        swprintf_s(defName, L"imsakiye_ramazan_%d.csv", h.year);
    } else {
        swprintf_s(defName, L"imsakiye_%04d_%02d.csv", viewYear_, viewMonth_);
    }
    lstrcpynW(fileBuf, defName, MAX_PATH);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"CSV (*.csv)\0*.csv\0Tüm Dosyalar (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"csv";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;

    AppLanguage lang = settings_.language;
    std::wostringstream ws;
    ws << settings_.cityName << L" (" << settings_.countryName << L")\r\n";
    ws << Tr(lang, StrId::ColDate) << L";" << Tr(lang, StrId::ColHijri) << L";";
    for (int i = 0; i < 6; ++i) {
        ws << GetEffectivePrayerName(static_cast<PrayerKind>(i), settings_) << (i == 5 ? L"\r\n" : L";");
    }

    for (size_t i = 0; i < rows_.size(); ++i) {
        const auto& r = rows_[i];
        wchar_t dBuf[64];
        swprintf_s(dBuf, L"%02d %ls %04d (%ls)", r.date.day,
                   GetShortMonthName(r.date.month, lang), r.date.year,
                   GetShortWeekdayName(r.date.DayOfWeek(), lang));
        ws << dBuf << L";" << FormatHijriDateLocalized(r.date, lang) << L";"
           << r.imsak.FormatHHMM() << L";" << r.gunes.FormatHHMM() << L";"
           << r.ogle.FormatHHMM() << L";" << r.ikindi.FormatHHMM() << L";"
           << r.aksam.FormatHHMM() << L";" << r.yatsi.FormatHHMM() << L"\r\n";
    }

    std::string utf8 = "\xEF\xBB\xBF" + WideToUtf8(ws.str());
    if (WriteFileAtomicallyUtf8(fileBuf, utf8)) {
        statusText_ = std::wstring(Tr(lang, StrId::StatusExportSaved)) + fileBuf;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ImsakiyeWindow::ExportHtml() {
    if (rows_.empty()) return;
    if (!licenseManager_.IsProActive()) {
        if (onRequestUpgrade_) onRequestUpgrade_();
        return;
    }
    wchar_t fileBuf[MAX_PATH]{};
    wchar_t defName[128];
    if (ramadanMode_) {
        HijriDateYMD h = GregorianToHijri(ramadanStart_);
        swprintf_s(defName, L"imsakiye_ramazan_%d.html", h.year);
    } else {
        swprintf_s(defName, L"imsakiye_%04d_%02d.html", viewYear_, viewMonth_);
    }
    lstrcpynW(fileBuf, defName, MAX_PATH);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"HTML (*.html)\0*.html\0Tüm Dosyalar (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"html";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;

    AppLanguage lang = settings_.language;
    std::wstring heading;
    if (ramadanMode_) {
        HijriDateYMD h = GregorianToHijri(ramadanStart_);
        heading = settings_.cityName + L" — " + Tr(lang, StrId::RamadanTitlePrefix) + L" " + std::to_wstring(h.year);
    } else {
        heading = settings_.cityName + L" — " + GetFullMonthName(viewMonth_, lang) + L" " + std::to_wstring(viewYear_);
    }

    std::wostringstream ws;
    ws << L"<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>" << HtmlEscape(heading) << L"</title>\n"
       << L"<style>body{font-family:'Segoe UI Variable Text','Segoe UI',sans-serif;margin:28px;color:#1b1e23}"
       << L"h2{margin:0 0 6px}p.sub{color:#5d6472;margin:0 0 18px}table{border-collapse:collapse;width:100%}"
       << L"th,td{border:1px solid #dce1e8;padding:7px 10px;text-align:center;font-size:13px}"
       << L"th{background:#0d7368;color:#fff}tr:nth-child(even){background:#f7f8fa}"
       << L"td.d{text-align:left;font-weight:600}</style></head><body>\n"
       << L"<h2>" << HtmlEscape(heading) << L"</h2><p class=\"sub\">" << HtmlEscape(settings_.countryName)
       << L" · VakitÇubuğu</p><table><thead><tr><th>" << HtmlEscape(Tr(lang, StrId::ColDate))
       << L"</th><th>" << HtmlEscape(Tr(lang, StrId::ColHijri)) << L"</th>";
    for (int i = 0; i < 6; ++i) {
        ws << L"<th>" << HtmlEscape(GetEffectivePrayerName(static_cast<PrayerKind>(i), settings_)) << L"</th>";
    }
    ws << L"</tr></thead><tbody>\n";

    for (const auto& r : rows_) {
        wchar_t dBuf[64];
        swprintf_s(dBuf, L"%02d %ls %04d, %ls", r.date.day,
                   GetShortMonthName(r.date.month, lang), r.date.year,
                   GetShortWeekdayName(r.date.DayOfWeek(), lang));
        ws << L"<tr><td class=\"d\">" << HtmlEscape(dBuf) << L"</td><td>"
           << HtmlEscape(FormatHijriDateLocalized(r.date, lang)) << L"</td>"
           << L"<td><b>" << r.imsak.FormatHHMM() << L"</b></td>"
           << L"<td>" << r.gunes.FormatHHMM() << L"</td>"
           << L"<td>" << r.ogle.FormatHHMM() << L"</td>"
           << L"<td>" << r.ikindi.FormatHHMM() << L"</td>"
           << L"<td><b>" << r.aksam.FormatHHMM() << L"</b></td>"
           << L"<td>" << r.yatsi.FormatHHMM() << L"</td></tr>\n";
    }
    ws << L"</tbody></table></body></html>\n";

    if (WriteFileAtomicallyUtf8(fileBuf, WideToUtf8(ws.str()))) {
        statusText_ = std::wstring(Tr(lang, StrId::StatusExportSaved)) + fileBuf;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void ImsakiyeWindow::ClampScroll(int clientH) {
    int listTop = static_cast<int>(std::llround(kHeaderDipY * scale_));
    int footerH = static_cast<int>(std::llround(kFooterDipH * scale_));
    int viewH = std::max(40, clientH - listTop - footerH);
    int totalH = static_cast<int>(rows_.size()) * static_cast<int>(std::llround(kRowDipH * scale_));
    int maxScroll = std::max(0, totalH - viewH);
    scrollY_ = std::clamp(scrollY_, 0, maxScroll);
}

void ImsakiyeWindow::UpdateScrollBar(int, int clientH) {
    if (!hwnd_) return;
    ClampScroll(clientH);

    int listTop = static_cast<int>(std::llround(kHeaderDipY * scale_));
    int footerH = static_cast<int>(std::llround(kFooterDipH * scale_));
    int viewH = std::max(40, clientH - listTop - footerH);
    int totalH = static_cast<int>(rows_.size()) * static_cast<int>(std::llround(kRowDipH * scale_));

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = std::max(0, totalH - 1);
    si.nPage = static_cast<UINT>(viewH);
    si.nPos = scrollY_;
    SetScrollInfo(hwnd_, SB_VERT, &si, TRUE);
}

ImsakiyeWindow::BtnId ImsakiyeWindow::HitTestToolbar(int x, int y) const {
    POINT pt{x, y};
    if (PtInRect(&tbRc_.prevRc, pt)) return BtnId::PrevMonth;
    if (PtInRect(&tbRc_.nextRc, pt)) return BtnId::NextMonth;
    if (PtInRect(&tbRc_.todayRc, pt)) return BtnId::Today;
    if (PtInRect(&tbRc_.ramadanRc, pt)) return BtnId::Ramadan;
    if (PtInRect(&tbRc_.syncRc, pt)) return BtnId::Sync;
    if (PtInRect(&tbRc_.csvRc, pt)) return BtnId::ExportCsv;
    if (PtInRect(&tbRc_.htmlRc, pt)) return BtnId::ExportHtml;
    return BtnId::None;
}

void ImsakiyeWindow::PaintClient(HDC hdc, int width, int height) {
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
            gdip::GdipSetCompositingQuality(g, gdip::kCompositingQualityHighQuality);

            bool isLight = IsAppLight(settings_.themeMode);
            uint32_t accent = GetWindowsAccentColorArgb(!isLight, settings_.useSystemAccentColor);
            AppLanguage lang = settings_.language;

            uint32_t bgSurf = isLight ? 0xFFF3F3F3u : 0xFF202020u;
            uint32_t cardBg = isLight ? 0xFFFFFFFFu : 0xFF2B2B2Bu;
            uint32_t altRowBg = isLight ? 0xFFF9FAFCu : 0xFF252629u;
            uint32_t borderCol = isLight ? 0x1E000000u : 0x22FFFFFFu;
            uint32_t textPrimary = isLight ? 0xFF1A1A1Au : 0xFFF5F7FAu;
            uint32_t textSecondary = isLight ? 0xFF5D6472u : 0xFFA6AEBCu;
            uint32_t btnBg = isLight ? 0xFFFFFFFFu : 0xFF303136u;
            uint32_t btnHover = isLight ? 0xFFE8ECF2u : 0xFF3C3E45u;
            uint32_t todayBg = WithAlpha(isLight ? 0x1Cu : 0x2Cu, accent);
            uint32_t gold = isLight ? 0xFFB86600u : 0xFFFFCE5Eu;
            uint32_t fridayBg = isLight ? 0x16D97706u : 0x18FBBF24u;

            gdip::GdipGraphicsClear(g, bgSurf);

            float s = static_cast<float>(scale_);
            float w = static_cast<float>(width);
            float h = static_cast<float>(height);
            float pad = 14.0f * s;

            void* uiFam = gdip::CreateBestUiFontFamily(false);
            void* dispFam = gdip::CreateBestUiFontFamily(true);
            void* iconFam = gdip::CreateFluentIconFontFamily();

            // Üst araç çubuğu
            float topY = 12.0f * s;
            float btnH = 28.0f * s;

            auto drawBtn = [&](RECT& outRc, float x, float bw, BtnId id, const wchar_t* label, bool highlighted = false) {
                outRc = {static_cast<LONG>(x), static_cast<LONG>(topY),
                         static_cast<LONG>(x + bw), static_cast<LONG>(topY + btnH)};
                uint32_t fill = highlighted ? WithAlpha(0x30u, accent) : (hoveredBtn_ == id ? btnHover : btnBg);
                uint32_t bdr = highlighted ? accent : borderCol;
                gdip::FillRoundedRect(g, x, topY, bw, btnH, 6.0f * s, fill);
                gdip::StrokeRoundedRect(g, x, topY, bw, btnH, 6.0f * s, bdr, 1.0f);
                gdip::DrawTextBox(g, label, uiFam, 11.2f * s, highlighted, highlighted ? accent : textPrimary,
                                  x, topY, bw, btnH, gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
            };

            drawBtn(tbRc_.prevRc, pad, 30.0f * s, BtnId::PrevMonth, L"◀");
            drawBtn(tbRc_.nextRc, pad + 34.0f * s, 30.0f * s, BtnId::NextMonth, L"▶");
            drawBtn(tbRc_.todayRc, pad + 70.0f * s, 68.0f * s, BtnId::Today, Tr(lang, StrId::BtnToday));
            drawBtn(tbRc_.ramadanRc, pad + 144.0f * s, 128.0f * s, BtnId::Ramadan,
                    ramadanMode_ ? Tr(lang, StrId::BtnMonthly) : Tr(lang, StrId::BtnRamadan), ramadanMode_);

            float rightX = w - pad;
            float htmlW = 96.0f * s;
            float csvW = 82.0f * s;
            float syncW = 78.0f * s;
            rightX -= htmlW;
            drawBtn(tbRc_.htmlRc, rightX, htmlW, BtnId::ExportHtml, Tr(lang, StrId::BtnExportHtml));
            rightX -= (csvW + 6.0f * s);
            drawBtn(tbRc_.csvRc, rightX, csvW, BtnId::ExportCsv, Tr(lang, StrId::BtnExportCsv));
            rightX -= (syncW + 6.0f * s);
            drawBtn(tbRc_.syncRc, rightX, syncW, BtnId::Sync,
                    syncBusy_ ? L"…" : Tr(lang, StrId::BtnSync));

            // Başlık + konum
            std::wstring periodTitle;
            if (ramadanMode_) {
                HijriDateYMD hRam = GregorianToHijri(ramadanStart_);
                periodTitle = std::wstring(Tr(lang, StrId::RamadanTitlePrefix)) + L" " + std::to_wstring(hRam.year);
            } else {
                periodTitle = std::wstring(GetFullMonthName(viewMonth_, lang)) + L" " + std::to_wstring(viewYear_);
            }
            std::wstring fullTitle = settings_.cityName + L" · " + periodTitle;
            gdip::DrawTextBox(g, fullTitle, dispFam, 14.5f * s, true, textPrimary,
                              pad, 44.0f * s, w - pad * 2.0f, 20.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            // Sabit Tablo Başlığı
            float listW = w - pad * 2.0f;
            float hdrY = 64.0f * s;
            float hdrH = 22.0f * s;
            gdip::FillRoundedRect(g, pad, hdrY, listW, hdrH, 5.0f * s, WithAlpha(isLight ? 0x1Au : 0x2Au, accent));

            float colDateW = listW * 0.21f;
            float colHijriW = listW * 0.21f;
            float colPrayerW = (listW - colDateW - colHijriW) / 6.0f;

            gdip::DrawTextBox(g, Tr(lang, StrId::ColDate), uiFam, 10.5f * s, true, textPrimary,
                              pad + 8.0f * s, hdrY, colDateW - 8.0f * s, hdrH,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            gdip::DrawTextBox(g, Tr(lang, StrId::ColHijri), uiFam, 10.5f * s, true, textPrimary,
                              pad + colDateW, hdrY, colHijriW, hdrH,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);
            for (int c = 0; c < 6; ++c) {
                float cx = pad + colDateW + colHijriW + c * colPrayerW;
                gdip::DrawTextBox(g, GetEffectivePrayerName(static_cast<PrayerKind>(c), settings_),
                                  uiFam, 10.5f * s, true, textPrimary,
                                  cx, hdrY, colPrayerW, hdrH,
                                  gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
            }

            // Satır alanı
            float listTop = static_cast<float>(kHeaderDipY) * s;
            float footerH = static_cast<float>(kFooterDipH) * s;
            float listBottom = h - footerH;
            float rowH = static_cast<float>(kRowDipH) * s;
            DateYMD today = ScheduleService::GetCurrentLocalDateTime().date;

            for (size_t i = 0; i < rows_.size(); ++i) {
                float ry = listTop + static_cast<float>(i) * rowH - static_cast<float>(scrollY_);
                if (ry + rowH < listTop || ry > listBottom) continue;

                const auto& r = rows_[i];
                bool isToday = (r.date == today);
                bool isFriday = (r.date.DayOfWeek() == 5);

                uint32_t rBg = isToday ? todayBg : (isFriday ? fridayBg : (i % 2 == 0 ? cardBg : altRowBg));
                gdip::FillRoundedRect(g, pad, ry + 1.0f, listW, rowH - 2.0f, 4.0f * s, rBg);
                if (isToday) {
                    gdip::StrokeRoundedRect(g, pad, ry + 1.0f, listW, rowH - 2.0f, 4.0f * s, accent, 1.2f);
                }

                wchar_t dBuf[64];
                if (ramadanMode_) {
                    swprintf_s(dBuf, L"%d %ls · %d %ls",
                               static_cast<int>(i) + 1, Tr(lang, StrId::RamadanDayPrefix),
                               r.date.day, GetShortMonthName(r.date.month, lang));
                } else {
                    swprintf_s(dBuf, L"%02d %ls · %ls",
                               r.date.day, GetShortMonthName(r.date.month, lang),
                               GetShortWeekdayName(r.date.DayOfWeek(), lang));
                }

                uint32_t dCol = isToday ? accent : (isFriday ? gold : textPrimary);
                gdip::DrawTextBox(g, dBuf, uiFam, 10.8f * s, isToday || isFriday, dCol,
                                  pad + 8.0f * s, ry, colDateW - 8.0f * s, rowH,
                                  gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

                gdip::DrawTextBox(g, FormatHijriDateLocalized(r.date, lang), uiFam, 10.2f * s, false, textSecondary,
                                  pad + colDateW, ry, colHijriW, rowH,
                                  gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

                TimeHM times[6] = {r.imsak, r.gunes, r.ogle, r.ikindi, r.aksam, r.yatsi};
                for (int c = 0; c < 6; ++c) {
                    float cx = pad + colDateW + colHijriW + c * colPrayerW;
                    bool ramAccent = ramadanMode_ && (c == 0 || c == 4);
                    uint32_t tCol = ramAccent ? accent : textPrimary;
                    gdip::DrawTextBox(g, times[c].FormatHHMM(), uiFam, 11.2f * s, isToday || ramAccent, tCol,
                                      cx, ry, colPrayerW, rowH,
                                      gdip::kStringAlignmentCenter, gdip::kStringAlignmentCenter);
                }
            }

            // Alt bilgi çubuğu
            gdip::FillRoundedRect(g, 0.0f, listBottom, w, footerH, 0.0f, bgSurf);
            const wchar_t* footerTxt = statusText_.empty() ? Tr(lang, StrId::ImsakiyeFooterHint) : statusText_.c_str();
            gdip::DrawTextBox(g, footerTxt, uiFam, 10.0f * s, false, textSecondary,
                              pad, listBottom + 2.0f * s, listW, footerH - 4.0f * s,
                              gdip::kStringAlignmentNear, gdip::kStringAlignmentCenter);

            if (iconFam) gdip::GdipDeleteFontFamily(iconFam);
            if (dispFam) gdip::GdipDeleteFontFamily(dispFam);
            if (uiFam) gdip::GdipDeleteFontFamily(uiFam);
            gdip::GdipDeleteGraphics(g);
        }
        gdip::GdipDisposeImage(gdiBmp);
    }

    BitBlt(hdc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDc);
}

LRESULT CALLBACK ImsakiyeWindow::WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ImsakiyeWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<ImsakiyeWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ImsakiyeWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    return self ? self->WndProc(hWnd, msg, wParam, lParam) : DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT ImsakiyeWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            if (mmi) {
                mmi->ptMinTrackSize.x = static_cast<LONG>(std::llround(600.0 * scale_));
                mmi->ptMinTrackSize.y = static_cast<LONG>(std::llround(440.0 * scale_));
            }
            return 0;
        }

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

        case WM_SIZE: {
            RECT rc{};
            GetClientRect(hWnd, &rc);
            UpdateScrollBar(rc.right - rc.left, rc.bottom - rc.top);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_VSCROLL: {
            RECT rc{};
            GetClientRect(hWnd, &rc);
            int clientH = rc.bottom - rc.top;
            SCROLLINFO si{};
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hWnd, SB_VERT, &si);
            int pos = si.nPos;
            int step = static_cast<int>(std::llround(kRowDipH * scale_));
            switch (LOWORD(wParam)) {
                case SB_LINEUP: pos -= step; break;
                case SB_LINEDOWN: pos += step; break;
                case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
                case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
                case SB_THUMBTRACK: pos = si.nTrackPos; break;
            }
            scrollY_ = pos;
            UpdateScrollBar(rc.right - rc.left, clientH);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            int step = static_cast<int>(std::llround(kRowDipH * scale_ * 2.0));
            scrollY_ -= (delta > 0 ? step : -step);
            RECT rc{};
            GetClientRect(hWnd, &rc);
            UpdateScrollBar(rc.right - rc.left, rc.bottom - rc.top);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!trackingLeave_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hWnd, 0};
                TrackMouseEvent(&tme);
                trackingLeave_ = true;
            }
            BtnId hit = HitTestToolbar(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            if (hit != hoveredBtn_) {
                hoveredBtn_ = hit;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            trackingLeave_ = false;
            if (hoveredBtn_ != BtnId::None) {
                hoveredBtn_ = BtnId::None;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONUP: {
            BtnId hit = HitTestToolbar(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
            if (hit == BtnId::PrevMonth) {
                ramadanMode_ = false;
                if (--viewMonth_ < 1) { viewMonth_ = 12; --viewYear_; }
                scrollY_ = 0;
                LoadRows();
                InvalidateRect(hWnd, nullptr, FALSE);
            } else if (hit == BtnId::NextMonth) {
                ramadanMode_ = false;
                if (++viewMonth_ > 12) { viewMonth_ = 1; ++viewYear_; }
                scrollY_ = 0;
                LoadRows();
                InvalidateRect(hWnd, nullptr, FALSE);
            } else if (hit == BtnId::Today) {
                LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
                ramadanMode_ = false;
                viewYear_ = now.date.year;
                viewMonth_ = now.date.month;
                scrollY_ = 0;
                LoadRows();
                InvalidateRect(hWnd, nullptr, FALSE);
            } else if (hit == BtnId::Ramadan) {
                ramadanMode_ = !ramadanMode_;
                if (ramadanMode_) {
                    LocalDateTime now = ScheduleService::GetCurrentLocalDateTime();
                    ramadanStart_ = FindNextOrCurrentRamadanStart(now.date);
                }
                scrollY_ = 0;
                LoadRows();
                InvalidateRect(hWnd, nullptr, FALSE);
            } else if (hit == BtnId::Sync) {
                StartBackgroundSync();
            } else if (hit == BtnId::ExportCsv) {
                ExportCsv();
            } else if (hit == BtnId::ExportHtml) {
                ExportHtml();
            }
            return 0;
        }

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                CloseNow();
                return 0;
            }
            break;

        case kMsgMonthSynced:
            syncBusy_ = false;
            statusText_ = wParam ? Tr(settings_.language, StrId::StatusSyncDone)
                                 : Tr(settings_.language, StrId::StatusSyncOffline);
            LoadRows();
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;

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
