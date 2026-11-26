#pragma once

#include <functional>
#include <string>

namespace vakit {

// Windows 11 Widgets board (Win+W) provider. Packaged builds only; call from the UI (STA) thread.
// dataSource returns the Adaptive Card data JSON; onOpen runs when the card is clicked.
bool RegisterWidgetProvider(std::function<std::wstring()> dataSource, std::function<void()> onOpen);
// Pushes fresh data to every pinned widget; skipped when nothing changed since the last push.
void PushWidgetUpdate();
void UnregisterWidgetProvider();

} // namespace vakit
