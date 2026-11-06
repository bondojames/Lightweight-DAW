// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/AudioEngine.h"
#include "engine/device/IAudioBackend.h"
#include "engine/render/OfflineRender.h"
#include "engine/telemetry/AudioTelemetry.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace ldaw::engine {

// What the UI can see. Copied out under a brief lock that the audio thread never touches.
struct AudioServiceState {
    AudioStatus status = AudioStatus::NotInitialized;
    std::string message; // Why it isn't running, in words for the user.
    std::vector<AudioDeviceInfo> devices;
    AudioStreamInfo stream;
    bool offlineTestRunning = false;
    std::string offlineTestResult;
};

// Runs slow device work on its own control thread, since opening a device can block
// for seconds and must not freeze the UI. It owns the engine and the backend, and
// neither depends on the renderer or the UI, so audio survives anything the GPU does.
class AudioService {
public:
    explicit AudioService(std::unique_ptr<IAudioBackend> backend);
    ~AudioService();
    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    // Each returns immediately; the work happens on the control thread.
    void open(const AudioStreamConfig& config);
    void refreshDevices();
    void runOfflineTest(const std::filesystem::path& wavPath);

    AudioServiceState state() const;
    std::uint64_t stateGeneration() const { return generation_.load(std::memory_order_relaxed); }

    // One thread only (the UI thread) may post commands.
    bool post(const EngineCommand& command) { return engine_.post(command); }
    EngineState engineState() const { return engine_.state(); }
    float takeOutputPeak() { return engine_.takeOutputPeak(); }
    AudioTelemetry& telemetry() { return telemetry_; }

private:
    void threadMain();
    void openNow();
    void updateDevices();
    void publish(const std::function<void(AudioServiceState&)>& change);

    std::unique_ptr<IAudioBackend> backend_;
    AudioEngine engine_;
    AudioTelemetry telemetry_;
    std::thread thread_;

    mutable std::mutex mutex_; // Never taken by the audio thread.
    std::condition_variable wake_;
    bool quit_ = false;
    bool refreshRequested_ = false;
    std::optional<AudioStreamConfig> pendingOpen_;
    std::optional<std::filesystem::path> pendingOfflineTest_;
    AudioServiceState state_;
    std::atomic<std::uint64_t> generation_{0};

    // Control thread only.
    AudioStreamConfig config_;
    bool wantOpen_ = false;
    std::string openedDefaultId_;
};

} // namespace ldaw::engine
