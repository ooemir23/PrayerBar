#pragma once

#include "taskbar_renderer.h"
#include "../core/models.h"
#include "../core/schedule_service.h"
#include "../core/settings_store.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace vakit {

class FlyoutWindow {
public:
    FlyoutWindow(
        SettingsStore& settingsStore,
        ScheduleService& scheduleService,
        std::function<void()> onOpenSettings,
        std::function<void()> onOpenImsakiye,
        std::function<void(bool)> onVisibilityChanged,
        std::function<void()> onForceRefresh);
    ~FlyoutWindow();

    bool IsOpen() const { return hwnd_ != nullptr && IsWindowVisible(hwnd_); }
    uint64_t LastClosedTick() const { return lastClosedTick_; }

    void ShowRightDocked(RECT anchorRect);
    void CloseNow();
    void RefreshView();

private:
    enum class HoverTarget { None, StopAudio, Imsakiye, Refresh, Settings };

    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void PaintClient(HDC hdc, int width, int height);
    void PositionRightDocked(RECT anchorRect, int winW, int winH);
    HoverTarget HitTestButton(int x, int y) const;
    void UpdateButtonsHover(int x, int y);

    SettingsStore& settingsStore_;
    ScheduleService& scheduleService_;
    std::function<void()> onOpenSettings_;
    std::function<void()> onOpenImsakiye_;
    std::function<void(bool)> onVisibilityChanged_;
    std::function<void()> onForceRefresh_;

    HWND hwnd_ = nullptr;
    double scale_ = 1.0;
    uint64_t lastClosedTick_ = 0;
    bool closing_ = false;
    bool trackingLeave_ = false;
    bool refreshBusy_ = false;
    bool autoOptimizeMemory_ = true;
    FlyoutDesignStyle openedDesign_ = FlyoutDesignStyle::CompactFluent;
    HoverTarget hovered_ = HoverTarget::None;

    AppSettings settings_{};  // loaded on open/refresh only; painting reads no files
    RECT prayerRc_[6]{};
    RECT btnStopRc_{};
    RECT btnImsakiyeRc_{};
    RECT btnRefreshRc_{};
    RECT btnSettingsRc_{};
};

PillRenderResult SmokeTestFlyoutRender(
    FlyoutDesignStyle design,
    const PrayerCountdownState& state,
    const AppSettings& settings,
    double scale,
    std::vector<uint32_t>* outPixels = nullptr,  // optional copy of the rendered BGRA pixels
    int* outWidth = nullptr,
    int* outHeight = nullptr);

} // namespace vakit
