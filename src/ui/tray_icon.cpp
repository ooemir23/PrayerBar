#include "tray_icon.h"
#include "taskbar_renderer.h"
#include "../core/formatter.h"
#include "../platform/audio_player.h"
#include "../platform/taskbar_geom.h"
#include "../platform/theme_utils.h"

#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace vakit {

namespace {

constexpr UINT kTrayIconId = 101;

void WriteFixedWide(wchar_t* dst, size_t maxChars, const std::wstring& text) {
    if (maxChars == 0) return;
    size_t n = std::min(text.size(), maxChars - 1);
    for (size_t i = 0; i < n; ++i) {
        dst[i] = text[i];
    }
    dst[n] = L'\0';
}

} // namespace

TrayIconHost::TrayIconHost(HWND ownerHwnd) : ownerHwnd_(ownerHwnd) {}

TrayIconHost::~TrayIconHost() {
    Remove();
}

void TrayIconHost::Update(const PrayerCountdownState& state, const AppSettings& settings, bool force) {
    bool shouldShow = settings.showTrayIcon && settings.trayStyle != TrayIconStyle::Hidden;
    if (!shouldShow) {
        if (!isTemporaryHidden_) {
            Remove();
        }
        return;
    }

    isTemporaryHidden_ = false;
    double scale = GetTaskbarScale();
    int iconSize = std::clamp(static_cast<int>(std::llround(16.0 * scale)), 16, 48);
    bool isLight = IsTaskbarLight(settings.themeMode);
    auto [top, bottom] = FormatTrayText(state, settings);
    std::wstring tooltip = FormatTrayTooltip(state, settings);
    int progressBucket = (settings.trayStyle == TrayIconStyle::MinutesRing ||
                          settings.trayStyle == TrayIconStyle::LargeDigits)
        ? static_cast<int>(std::llround(state.progress * 36.0))
        : 0;
    const bool audioPlaying = AudioPlayer::IsPlaying();
    const int nowMin = static_cast<int>(state.calculatedAt.ToEpochSeconds() / 60);
    const bool isMuted = !settings.notificationsEnabled || (settings.notifyMuteUntilMin > nowMin);
    const bool criticalUrgent = state.isUrgent && state.remainingSeconds <= 5 * 60;
    const bool kerahatActive = !state.isUrgent && settings.showKerahatIndicator && state.kerahatStatus != KerahatStatus::None;

    wchar_t sigBuf[320];
    swprintf_s(sigBuf, L"%d|%d|%d|%d|%d|%d|%d|%ls|%ls|%d|%d|%d|%d|%d|%d|%ls",
               shouldShow ? 1 : 0, iconSize, isLight ? 1 : 0,
               settings.useSystemAccentColor ? 1 : 0,
               static_cast<int>(settings.trayStyle),
               settings.trayShowMinutesUpTo99 ? 1 : 0,
               static_cast<int>(state.nextPrayer), top.c_str(), bottom.c_str(),
               state.isUrgent ? 1 : 0, criticalUrgent ? 1 : 0,
               kerahatActive ? 1 : 0, audioPlaying ? 1 : 0, isMuted ? 1 : 0,
               progressBucket, tooltip.c_str());
    std::wstring sig(sigBuf);

    if (!force && isAdded_ && sig == lastSignature_) {
        return;
    }
    lastSignature_ = std::move(sig);

    HICON newIcon = CreateTrayIconHandle(state, settings, iconSize, isLight);
    if (!newIcon) return;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = ownerHwnd_;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_STATE;
    nid.uCallbackMessage = kCallbackMessageId;
    nid.hIcon = newIcon;
    nid.dwState = 0;
    nid.dwStateMask = NIS_HIDDEN;
    WriteFixedWide(nid.szTip, 128, tooltip);

    BOOL ok = Shell_NotifyIconW(isAdded_ ? NIM_MODIFY : NIM_ADD, &nid);
    if (!ok && isAdded_) {
        ok = Shell_NotifyIconW(NIM_ADD, &nid);
    }

    if (ok) {
        isAdded_ = true;
        if (currentIcon_) {
            DestroyIcon(currentIcon_);
        }
        currentIcon_ = newIcon;
    } else {
        DestroyIcon(newIcon);
    }
}

void TrayIconHost::ShowNotification(const std::wstring& title, const std::wstring& body, bool silent) {
    if (!isAdded_) {
        NOTIFYICONDATAW nidTemp{};
        nidTemp.cbSize = sizeof(nidTemp);
        nidTemp.hWnd = ownerHwnd_;
        nidTemp.uID = kTrayIconId;
        nidTemp.uFlags = NIF_MESSAGE | NIF_ICON | NIF_STATE | NIF_INFO;
        nidTemp.uCallbackMessage = kCallbackMessageId;
        nidTemp.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
        if (!nidTemp.hIcon) {
            nidTemp.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        }
        nidTemp.dwState = NIS_HIDDEN;
        nidTemp.dwStateMask = NIS_HIDDEN;
        nidTemp.dwInfoFlags = NIIF_INFO | (silent ? NIIF_NOSOUND : 0u);
        WriteFixedWide(nidTemp.szInfoTitle, 64, title);
        WriteFixedWide(nidTemp.szInfo, 256, body);
        if (Shell_NotifyIconW(NIM_ADD, &nidTemp)) {
            isAdded_ = true;
            isTemporaryHidden_ = true;
        }
        return;
    }

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = ownerHwnd_;
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO | (silent ? NIIF_NOSOUND : 0u);
    WriteFixedWide(nid.szInfoTitle, 64, title);
    WriteFixedWide(nid.szInfo, 256, body);

    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayIconHost::CleanupTemporaryHiddenIcon() {
    if (isTemporaryHidden_) {
        Remove();
    }
}

void TrayIconHost::RecreateIfNeeded(const PrayerCountdownState& state, const AppSettings& settings) {
    isAdded_ = false;
    isTemporaryHidden_ = false;
    lastSignature_.clear();
    Update(state, settings, true);
}

void TrayIconHost::Remove() {
    if (isAdded_) {
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = ownerHwnd_;
        nid.uID = kTrayIconId;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        isAdded_ = false;
    }

    isTemporaryHidden_ = false;
    if (currentIcon_) {
        DestroyIcon(currentIcon_);
        currentIcon_ = nullptr;
    }
    lastSignature_.clear();
}

} // namespace vakit
