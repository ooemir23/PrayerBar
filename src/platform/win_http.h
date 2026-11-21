#pragma once

#include <optional>
#include <string>

namespace vakit {

std::optional<std::string> WinHttpGetUtf8(const std::wstring& host, const std::wstring& pathAndQuery, bool useHttps = true, int timeoutMs = 8000);
std::optional<std::string> WinHttpPostFormUtf8(const std::wstring& host, const std::wstring& path, const std::string& formBody, int timeoutMs = 8000);

} // namespace vakit
