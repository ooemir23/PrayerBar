#pragma once

#include <functional>
#include <string>

namespace vakit {

// Windows 10/11 toast with optional "Mute Adhan" button and automatic audio stop on user dismissal.
bool ShowModernToast(
    const std::wstring& title,
    const std::wstring& body,
    bool silent,
    const std::wstring& stopButtonLabel = L"",
    std::function<void()> onStopAudio = nullptr);

} // namespace vakit
