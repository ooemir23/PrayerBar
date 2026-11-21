#pragma once

#include "../core/models.h"
#include <string>

namespace vakit {

class AudioPlayer {
public:
    static void Play(NotificationSoundKind kind, const std::wstring& customFilePath, int volumePercent);
    static void Stop();
    static bool IsPlaying();
    static bool SmokeTestBundledAudioPaths();
    static std::wstring ResolveBundledSoundPath(NotificationSoundKind kind);
};

} // namespace vakit
