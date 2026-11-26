#include "audio_player.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace vakit {

namespace {

constexpr wchar_t kMciAlias[] = L"vakit_audio";

std::mutex g_queueMutex;
std::condition_variable g_queueCv;
std::deque<std::function<void()>> g_tasks;
bool g_threadStarted = false;
std::atomic<bool> g_mciOpen{false};
std::vector<uint8_t> g_activeWavBuf;

void EnsureAudioThread() {
    std::lock_guard<std::mutex> lk(g_queueMutex);
    if (g_threadStarted) return;
    g_threadStarted = true;
    std::thread([]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> ul(g_queueMutex);
                g_queueCv.wait(ul, [] { return !g_tasks.empty(); });
                task = std::move(g_tasks.front());
                g_tasks.pop_front();
            }
            if (task) {
                task();
            }
        }
    }).detach();
}

void EnqueueAudioTask(std::function<void()> fn) {
    EnsureAudioThread();
    {
        std::lock_guard<std::mutex> lk(g_queueMutex);
        g_tasks.push_back(std::move(fn));
    }
    g_queueCv.notify_one();
}

void CloseMciCore() {
    if (!g_mciOpen.load()) return;
    mciSendStringW(L"stop vakit_audio", nullptr, 0, nullptr);
    mciSendStringW(L"close vakit_audio", nullptr, 0, nullptr);
    g_mciOpen.store(false);
}

void WriteWavHeader(std::vector<uint8_t>& buf, uint32_t sampleRate, uint32_t dataBytes) {
    auto writeU32 = [&](uint32_t v) {
        size_t p = buf.size();
        buf.resize(p + 4);
        std::memcpy(buf.data() + p, &v, 4);
    };
    auto writeU16 = [&](uint16_t v) {
        size_t p = buf.size();
        buf.resize(p + 2);
        std::memcpy(buf.data() + p, &v, 2);
    };
    auto writeTag = [&](const char* tag) {
        buf.insert(buf.end(), tag, tag + 4);
    };

    buf.reserve(44 + dataBytes);
    writeTag("RIFF");
    writeU32(36 + dataBytes);
    writeTag("WAVE");
    writeTag("fmt ");
    writeU32(16);
    writeU16(1);
    writeU16(1);
    writeU32(sampleRate);
    writeU32(sampleRate * 2);
    writeU16(2);
    writeU16(16);
    writeTag("data");
    writeU32(dataBytes);
}

std::vector<uint8_t> BuildGentleChimeWav(double volumeScale) {
    constexpr uint32_t sampleRate = 44100;
    constexpr double durationSec = 0.90;
    uint32_t totalSamples = static_cast<uint32_t>(sampleRate * durationSec);
    std::vector<uint8_t> wav;
    WriteWavHeader(wav, sampleRate, totalSamples * 2);

    constexpr double kTwoPi = 6.283185307179586;
    for (uint32_t i = 0; i < totalSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double note1 = std::sin(kTwoPi * 587.33 * t) * 0.72 +
                       std::sin(kTwoPi * 1174.66 * t) * 0.14;
        double env1 = t < 0.015 ? (t / 0.015) : std::exp(-4.8 * (t - 0.015));

        double t2 = t - 0.19;
        double note2 = 0.0;
        double env2 = 0.0;
        if (t2 > 0.0) {
            note2 = std::sin(kTwoPi * 880.00 * t2) * 0.68 +
                    std::sin(kTwoPi * 1760.00 * t2) * 0.12;
            env2 = t2 < 0.015 ? (t2 / 0.015) : std::exp(-4.4 * (t2 - 0.015));
        }

        double sample = (note1 * env1 * 0.55 + note2 * env2 * 0.60) * 0.35 * volumeScale;
        int16_t pcm = static_cast<int16_t>(std::clamp(sample * 32767.0, -32768.0, 32767.0));
        wav.push_back(static_cast<uint8_t>(pcm & 0xFF));
        wav.push_back(static_cast<uint8_t>((pcm >> 8) & 0xFF));
    }
    return wav;
}

std::vector<uint8_t> BuildWarmBellWav(double volumeScale) {
    constexpr uint32_t sampleRate = 44100;
    constexpr double durationSec = 1.45;
    uint32_t totalSamples = static_cast<uint32_t>(sampleRate * durationSec);
    std::vector<uint8_t> wav;
    WriteWavHeader(wav, sampleRate, totalSamples * 2);

    constexpr double kTwoPi = 6.283185307179586;
    for (uint32_t i = 0; i < totalSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double env = t < 0.02 ? (t / 0.02) : std::exp(-2.9 * (t - 0.02));
        double tone = std::sin(kTwoPi * 392.00 * t) * 0.65 +
                      std::sin(kTwoPi * 587.33 * t) * 0.25 +
                      std::sin(kTwoPi * 783.99 * t) * 0.15;

        double sample = tone * env * 0.36 * volumeScale;
        int16_t pcm = static_cast<int16_t>(std::clamp(sample * 32767.0, -32768.0, 32767.0));
        wav.push_back(static_cast<uint8_t>(pcm & 0xFF));
        wav.push_back(static_cast<uint8_t>((pcm >> 8) & 0xFF));
    }
    return wav;
}

void PlayMemoryWavSyncOnAudioThread(std::vector<uint8_t> wav) {
    PlaySoundW(nullptr, nullptr, 0);
    CloseMciCore();
    g_activeWavBuf = std::move(wav);
    PlaySoundW(reinterpret_cast<LPCWSTR>(g_activeWavBuf.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

std::wstring GetExeDirectory() {
    wchar_t pathBuf[MAX_PATH]{};
    DWORD len = GetModuleFileNameW(nullptr, pathBuf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L".";
    std::filesystem::path p(pathBuf);
    return p.parent_path().wstring();
}

} // namespace

std::wstring AudioPlayer::ResolveBundledSoundPath(NotificationSoundKind kind) {
    std::wstring fileName;
    if (kind == NotificationSoundKind::AdhanShort) fileName = L"adhan_short.mp3";
    else if (kind == NotificationSoundKind::AdhanMakkah) fileName = L"adhan_makkah.mp3";
    else return {};

    std::wstring exeDir = GetExeDirectory();
    std::wstring p1 = exeDir + L"\\Assets\\Sounds\\" + fileName;
    if (std::filesystem::exists(p1)) return p1;

    std::wstring p2 = exeDir + L"\\assets\\sounds\\" + fileName;
    if (std::filesystem::exists(p2)) return p2;

    std::wstring p3 = exeDir + L"\\..\\assets\\sounds\\" + fileName;
    if (std::filesystem::exists(p3)) return p3;

    return p1;
}

bool AudioPlayer::SmokeTestBundledAudioPaths() {
    std::wstring p1 = ResolveBundledSoundPath(NotificationSoundKind::AdhanShort);
    std::wstring p2 = ResolveBundledSoundPath(NotificationSoundKind::AdhanMakkah);
    std::error_code ec;
    return !p1.empty() && std::filesystem::exists(p1, ec) && std::filesystem::file_size(p1, ec) > 10000 &&
           !p2.empty() && std::filesystem::exists(p2, ec) && std::filesystem::file_size(p2, ec) > 10000;
}

bool AudioPlayer::IsPlaying() {
    if (!g_mciOpen.load()) return false;
    auto promise = std::make_shared<std::promise<bool>>();
    auto fut = promise->get_future();
    EnqueueAudioTask([promise]() {
        if (!g_mciOpen.load()) {
            promise->set_value(false);
            return;
        }
        wchar_t buf[64]{};
        MCIERROR rc = mciSendStringW(L"status vakit_audio mode", buf, 64, nullptr);
        if (rc != 0) {
            g_mciOpen.store(false);
            promise->set_value(false);
            return;
        }
        bool playing = (_wcsicmp(buf, L"playing") == 0);
        if (!playing) {
            CloseMciCore();
        }
        promise->set_value(playing);
    });

    if (fut.wait_for(std::chrono::milliseconds(250)) == std::future_status::ready) {
        return fut.get();
    }
    return false;
}

void AudioPlayer::Play(NotificationSoundKind kind, const std::wstring& customFilePath, int volumePercent) {
    int clampedVol = std::clamp(volumePercent, 0, 100);
    if (kind == NotificationSoundKind::None || clampedVol == 0) return;

    double volScale = clampedVol / 100.0;
    if (kind == NotificationSoundKind::GentleChime) {
        EnqueueAudioTask([volScale]() { PlayMemoryWavSyncOnAudioThread(BuildGentleChimeWav(volScale)); });
        return;
    }
    if (kind == NotificationSoundKind::WarmBell) {
        EnqueueAudioTask([volScale]() { PlayMemoryWavSyncOnAudioThread(BuildWarmBellWav(volScale)); });
        return;
    }

    std::wstring filePath = (kind == NotificationSoundKind::CustomFile)
        ? customFilePath
        : ResolveBundledSoundPath(kind);

    std::error_code ec;
    if (filePath.empty() || !std::filesystem::exists(filePath, ec)) {
        EnqueueAudioTask([volScale]() { PlayMemoryWavSyncOnAudioThread(BuildGentleChimeWav(volScale)); });
        return;
    }

    EnqueueAudioTask([filePath, clampedVol, volScale]() {
        PlaySoundW(nullptr, nullptr, 0);
        CloseMciCore();

        std::wstring openCmd = L"open \"" + filePath + L"\" type mpegvideo alias " + kMciAlias;
        MCIERROR rc = mciSendStringW(openCmd.c_str(), nullptr, 0, nullptr);
        if (rc != 0) {
            std::wstring fallbackCmd = L"open \"" + filePath + L"\" alias " + kMciAlias;
            rc = mciSendStringW(fallbackCmd.c_str(), nullptr, 0, nullptr);
        }
        if (rc != 0) {
            PlayMemoryWavSyncOnAudioThread(BuildGentleChimeWav(volScale));
            return;
        }

        g_mciOpen.store(true);
        wchar_t volCmd[64];
        swprintf_s(volCmd, L"setaudio %ls volume to %d", kMciAlias, clampedVol * 10);
        mciSendStringW(volCmd, nullptr, 0, nullptr);

        wchar_t playCmd[64];
        swprintf_s(playCmd, L"play %ls", kMciAlias);
        mciSendStringW(playCmd, nullptr, 0, nullptr);
    });
}

void AudioPlayer::Stop() {
    PlaySoundW(nullptr, nullptr, 0);
    if (g_threadStarted || g_mciOpen.load()) {
        EnqueueAudioTask([]() {
            PlaySoundW(nullptr, nullptr, 0);
            CloseMciCore();
            g_activeWavBuf.clear();
            g_activeWavBuf.shrink_to_fit();
        });
    }
}

} // namespace vakit
