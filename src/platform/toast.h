#pragma once

#include <string>

namespace vakit {

// Windows 10/11 toast with system Snooze + Dismiss buttons (Windows itself re-shows snoozed toasts,
// so nothing keeps running in this process). Returns false when toasts are unavailable
// (e.g. unpackaged build without an AppUserModelID); callers then fall back to the tray balloon.
bool ShowModernToast(const std::wstring& title, const std::wstring& body, bool silent);

} // namespace vakit
