#pragma once

#include "models.h"
#include <optional>
#include <string>

namespace vakit {

class SettingsStore {
public:
    explicit SettingsStore(std::wstring customBaseDir = {});

    const std::wstring& BaseDir() const { return baseDir_; }
    const std::wstring& SettingsPath() const { return settingsPath_; }

    AppSettings Load() const;
    bool Save(const AppSettings& settings) const;
    bool ExportToFile(const AppSettings& settings, const std::wstring& filePath) const;
    std::optional<AppSettings> ImportFromFile(const std::wstring& filePath) const;

private:
    std::wstring baseDir_;
    std::wstring settingsPath_;
};

bool WriteFileAtomicallyUtf8(const std::wstring& path, const std::string& content);
bool ReadFileUtf8(const std::wstring& path, std::string& outContent);
void EnsureDirectoryRecursive(const std::wstring& dir);
std::wstring GetDefaultLocalAppDataDir();

} // namespace vakit
