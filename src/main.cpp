#include "core/formatter.h"
#include "core/license_manager.h"
#include "core/models.h"
#include "core/schedule_service.h"
#include "core/settings_store.h"
#include "core/utf_util.h"
#include "platform/audio_player.h"
#include "platform/theme_utils.h"
#include "ui/app_host.h"
#include "ui/gdiplus_flat.h"
#include "ui/taskbar_renderer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

bool SaveTrayPreviewBmp(const std::wstring& bmpPath, const vakit::PrayerCountdownState& state, const vakit::AppSettings& settings) {
    constexpr int W = 280;
    constexpr int H = 120;
    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return false;
    HDC memDc = CreateCompatibleDC(screenDc);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (hBmp && bits) {
        HGDIOBJ old = SelectObject(memDc, hBmp);
        auto* px = static_cast<uint32_t*>(bits);
        for (int y = 0; y < H; ++y) {
            uint32_t bg = (y < H / 2) ? 0xFF242424u : 0xFFF3F3F3u;
            for (int x = 0; x < W; ++x) px[y * W + x] = bg;
        }
        GdiFlush();

        vakit::AppSettings copy = settings;
        for (int row = 0; row < 2; ++row) {
            bool isLight = (row == 1);
            int y32 = (row == 0 ? 14 : 74);
            int y48 = (row == 0 ? 6 : 66);
            for (int t = 0; t < 3; ++t) {
                copy.trayStyle = static_cast<vakit::TrayIconStyle>(t);
                int xBase = 14 + t * 90;
                if (HICON h32 = vakit::CreateTrayIconHandle(state, copy, 32, isLight)) {
                    DrawIconEx(memDc, xBase, y32, h32, 32, 32, 0, nullptr, DI_NORMAL);
                    DestroyIcon(h32);
                }
                if (HICON h48 = vakit::CreateTrayIconHandle(state, copy, 48, isLight)) {
                    DrawIconEx(memDc, xBase + 38, y48, h48, 48, 48, 0, nullptr, DI_NORMAL);
                    DestroyIcon(h48);
                }
            }
        }
        GdiFlush();

        std::filesystem::path p(bmpPath);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }

        HANDLE f = CreateFileW(bmpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            uint32_t imgSize = static_cast<uint32_t>(W * H * 4);
            BITMAPFILEHEADER bfh{};
            bfh.bfType = 0x4D42;
            bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            bfh.bfSize = bfh.bfOffBits + imgSize;

            BITMAPINFOHEADER bih{};
            bih.biSize = sizeof(BITMAPINFOHEADER);
            bih.biWidth = W;
            bih.biHeight = H;
            bih.biPlanes = 1;
            bih.biBitCount = 32;
            bih.biCompression = BI_RGB;
            bih.biSizeImage = imgSize;

            DWORD written = 0;
            WriteFile(f, &bfh, sizeof(bfh), &written, nullptr);
            WriteFile(f, &bih, sizeof(bih), &written, nullptr);
            for (int y = H - 1; y >= 0; --y) {
                for (int x = 0; x < W; ++x) px[y * W + x] |= 0xFF000000u;
                WriteFile(f, px + y * W, static_cast<DWORD>(W * 4), &written, nullptr);
            }
            CloseHandle(f);
            ok = true;
        }
        SelectObject(memDc, old);
        DeleteObject(hBmp);
    }
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
    return ok;
}

int RunDiagnosticSmoke(const std::wstring& previewBmpPath, const std::wstring& verifyKey) {
    wchar_t tmpRoot[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpRoot);
    std::wstring tempDir = std::wstring(tmpRoot) + L"VakitCubuguSmoke_" + std::to_wstring(GetCurrentProcessId());

    vakit::SettingsStore store(tempDir);
    vakit::AppSettings settings = store.Load();
    settings.useOnlineSync = false;
    settings.language = vakit::AppLanguage::Turkish;

    vakit::ScheduleService schedule(tempDir + L"\\cache");
    vakit::LocalDateTime now{{2026, 11, 21}, 15, 2, 0};
    vakit::PrayerCountdownState state = schedule.ComputeState(now, settings);

    printf("City: %s, Next: %s at %02d:%02d, RemMin: %.1f\n",
           vakit::WideToUtf8(settings.cityName).c_str(),
           vakit::WideToUtf8(vakit::GetPrayerDisplayName(state.nextPrayer)).c_str(),
           state.nextPrayerDateTime.hour, state.nextPrayerDateTime.minute,
           static_cast<double>(state.remainingSeconds) / 60.0);

    for (double scale : {1.0, 1.5}) {
        for (int i = 0; i <= 8; ++i) {
            settings.displayStyle = static_cast<vakit::TaskbarDisplayStyle>(i);
            vakit::FormattedTaskbarDisplay disp = vakit::FormatTaskbar(state, settings);
            int w = vakit::MeasureDesiredPillWidth(disp, settings, scale);
            bool opaque = (i == 0 && scale > 1.0);
            auto r = vakit::SmokeTestPillRender(disp, settings, scale, opaque);
            printf("  [%.1fx] style=%d -> w=%3dpx status=%d px=%d | '%s' / '%s'\n",
                   scale, i, w, r.status, r.nonZeroPixels,
                   vakit::WideToUtf8(disp.line1).c_str(),
                   disp.hasLine2 ? vakit::WideToUtf8(disp.line2).c_str() : "-");
            if (r.status != 0 || r.nonZeroPixels <= 0) {
                std::error_code ec;
                std::filesystem::remove_all(tempDir, ec);
                vakit::gdip::Shutdown();
                return 4;
            }
        }

        for (int t = 0; t < 3; ++t) {
            settings.trayStyle = static_cast<vakit::TrayIconStyle>(t);
            HICON hIcon = vakit::CreateTrayIconHandle(state, settings, static_cast<int>(16 * scale), false);
            if (!hIcon) {
                std::error_code ec;
                std::filesystem::remove_all(tempDir, ec);
                vakit::gdip::Shutdown();
                return 2;
            }
            DestroyIcon(hIcon);
        }

        for (int f = 0; f <= 3; ++f) {
            auto design = static_cast<vakit::FlyoutDesignStyle>(f);
            auto fr = vakit::SmokeTestFlyoutRender(design, state, settings, scale);
            printf("  [%.1fx] flyout=%d -> status=%d px=%d\n", scale, f, fr.status, fr.nonZeroPixels);
            if (fr.status != 0 || fr.nonZeroPixels <= 0) {
                std::error_code ec;
                std::filesystem::remove_all(tempDir, ec);
                vakit::gdip::Shutdown();
                return 3;
            }
        }
    }

    auto monthRows = schedule.GetMonthSchedule(2026, 11, settings, false);
    if (monthRows.size() != 30) {
        fprintf(stderr, "Imsakiye month count mismatch: %zu\n", monthRows.size());
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        vakit::gdip::Shutdown();
        return 6;
    }

    vakit::AppSettings customTest = settings;
    customTest.displayStyle = vakit::TaskbarDisplayStyle::TwoLineShort;
    customTest.customPrayerNames[static_cast<size_t>(vakit::PrayerKind::Ikindi)] = L"İkindi Özel";
    auto customDisp = vakit::FormatTaskbar(state, customTest);
    if (customDisp.line1 != L"İkindi Özel") {
        fprintf(stderr, "Custom prayer name check failed.\n");
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        vakit::gdip::Shutdown();
        return 7;
    }

    if (!vakit::AudioPlayer::SmokeTestBundledAudioPaths()) {
        fprintf(stderr, "Bundled Adhan MP3 files missing or invalid.\n");
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        vakit::gdip::Shutdown();
        return 5;
    }

    if (!verifyKey.empty()) {
        vakit::LicenseManager lm(tempDir + L"\\lic", false);
        auto vr = lm.ActivateKey(verifyKey, {2026, 11, 21}, false);
        if (!vr.ok) {
            fprintf(stderr, "Key verification failed: %s\n", vakit::WideToUtf8(vr.errorCode).c_str());
            std::error_code ec;
            std::filesystem::remove_all(tempDir, ec);
            vakit::gdip::Shutdown();
            return 9;
        }
        const char* tierName = (vr.tier == vakit::LicenseTier::ProLifetime) ? "ProLifetime" :
                               (vr.tier == vakit::LicenseTier::ProAnnual)   ? "ProAnnual" : "Pro";
        printf("KeyVerified=%s\n", tierName);
    }

    if (!previewBmpPath.empty()) {
        if (!SaveTrayPreviewBmp(previewBmpPath, state, settings)) {
            fprintf(stderr, "Failed saving tray preview BMP.\n");
            std::error_code ec;
            std::filesystem::remove_all(tempDir, ec);
            vakit::gdip::Shutdown();
            return 8;
        }
        printf("Tray preview written.\n");
    }

    vakit::TrimWorkingSet();
    vakit::MemoryStats mem = vakit::GetMemoryStats();
    printf("Memory: WorkingSet=%.2f MB, Private=%.2f MB, Handles=%lu\n",
           mem.workingSetMb, mem.privateMb, mem.handleCount);

    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    vakit::gdip::Shutdown();
    return 0;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool isSmoke = false;
    std::wstring previewPath;
    std::wstring verifyKey;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (_wcsicmp(argv[i], L"--diag-smoke") == 0) {
                isSmoke = true;
            } else if (_wcsicmp(argv[i], L"--tray-preview") == 0 && (i + 1) < argc) {
                previewPath = argv[++i];
            } else if (_wcsicmp(argv[i], L"--verify-key") == 0 && (i + 1) < argc) {
                verifyKey = argv[++i];
            }
        }
        LocalFree(argv);
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_WIN95_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    if (isSmoke) {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        bool hasRedirectedOut = (hOut != nullptr && hOut != INVALID_HANDLE_VALUE && GetFileType(hOut) != FILE_TYPE_UNKNOWN);
        if (!hasRedirectedOut && AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
        }
        SetConsoleOutputCP(CP_UTF8);
        int code = RunDiagnosticSmoke(previewPath, verifyKey);
        fflush(stdout);
        fflush(stderr);
        CoUninitialize();
        return code;
    }

    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Local\\VakitCubugu_SingleInstance_v1");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(vakit::AppHost::kHostWindowClassName, nullptr);
        if (existing) {
            PostMessageW(existing, vakit::AppHost::kMsgShowFlyout, 0, 0);
        }
        CloseHandle(hMutex);
        CoUninitialize();
        return 0;
    }

    vakit::SettingsStore settingsStore;
    vakit::ScheduleService scheduleService;
    int exitCode = 0;
    {
        vakit::AppHost host(settingsStore, scheduleService);
        exitCode = host.Run();
    }

    if (hMutex) CloseHandle(hMutex);
    CoUninitialize();
    return exitCode;
}
