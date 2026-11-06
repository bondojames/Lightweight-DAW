// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <objbase.h>

#include "engine/device/AudioService.h"

#include <algorithm>
#include <chrono>
#include <functional>

namespace ldaw::engine {
namespace {

constexpr auto kPollInterval = std::chrono::milliseconds(500);
constexpr auto kRetryInterval = std::chrono::seconds(2);
constexpr double kOfflineTestRate = 48000.0;
constexpr double kOfflineTestBpm = 120.0;
constexpr double kOfflineTestSeconds = 16.0;

std::string defaultDeviceId(const std::vector<AudioDeviceInfo>& devices) {
    const auto it = std::find_if(devices.begin(), devices.end(), [](const AudioDeviceInfo& d) { return d.isDefault; });
    return it == devices.end() ? std::string() : it->id;
}

} // namespace

AudioService::AudioService(std::unique_ptr<IAudioBackend> backend) : backend_(std::move(backend)) {
    thread_ = std::thread([this] { threadMain(); });
}

AudioService::~AudioService() {
    {
        const std::scoped_lock lock(mutex_);
        quit_ = true;
    }
    wake_.notify_all();
    thread_.join();
}

void AudioService::open(const AudioStreamConfig& config) {
    {
        const std::scoped_lock lock(mutex_);
        pendingOpen_ = config;
    }
    wake_.notify_all();
}

void AudioService::refreshDevices() {
    {
        const std::scoped_lock lock(mutex_);
        refreshRequested_ = true;
    }
    wake_.notify_all();
}

void AudioService::runOfflineTest(const std::filesystem::path& wavPath) {
    {
        const std::scoped_lock lock(mutex_);
        pendingOfflineTest_ = wavPath;
    }
    wake_.notify_all();
}

AudioServiceState AudioService::state() const {
    const std::scoped_lock lock(mutex_);
    return state_;
}

void AudioService::publish(const std::function<void(AudioServiceState&)>& change) {
    {
        const std::scoped_lock lock(mutex_);
        change(state_);
    }
    generation_.fetch_add(1, std::memory_order_relaxed);
}

void AudioService::updateDevices() {
    auto devices = backend_->enumerateOutputs();
    publish([&](AudioServiceState& state) { state.devices = std::move(devices); });
}

void AudioService::openNow() {
    backend_->stop();
    auto devices = backend_->enumerateOutputs();
    const std::string defaultId = defaultDeviceId(devices);
    AudioStreamConfig config = config_;
    std::string note;

    if (devices.empty()) {
        publish([&](AudioServiceState& state) {
            state.devices = std::move(devices);
            state.status = AudioStatus::Failed;
            state.stream = {};
            state.message = "No audio output device found. Connect one, or run the offline test from the Audio menu.";
        });
        return;
    }
    if (!config.outputDeviceId.empty()) {
        const bool present = std::any_of(devices.begin(), devices.end(), [&](const AudioDeviceInfo& d) { return d.id == config.outputDeviceId; });
        if (!present) {
            config.outputDeviceId.clear();
            note = "The chosen audio device isn't connected, so the Windows default is being used.";
        }
    }

    const bool started = backend_->start(config, engine_, telemetry_);
    openedDefaultId_ = defaultId;
    publish([&](AudioServiceState& state) {
        state.devices = std::move(devices);
        state.status = backend_->status();
        state.stream = started ? backend_->streamInfo() : AudioStreamInfo{};
        state.message = started ? note : backend_->lastError();
    });
}

void AudioService::threadMain() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    auto nextRetry = std::chrono::steady_clock::now();
    updateDevices();

    for (;;) {
        std::optional<AudioStreamConfig> openRequest;
        std::optional<std::filesystem::path> offlineRequest;
        bool refresh = false;
        {
            std::unique_lock lock(mutex_);
            wake_.wait_for(lock, kPollInterval, [this] { return quit_ || pendingOpen_ || pendingOfflineTest_ || refreshRequested_; });
            if (quit_) break;
            openRequest.swap(pendingOpen_);
            offlineRequest.swap(pendingOfflineTest_);
            refresh = refreshRequested_;
            refreshRequested_ = false;
        }

        if (openRequest) {
            config_ = *openRequest;
            wantOpen_ = true;
            openNow();
            nextRetry = std::chrono::steady_clock::now() + kRetryInterval;
        }

        const bool devicesChanged = backend_->takeDevicesChanged();
        if (devicesChanged || refresh) {
            updateDevices();
            if (wantOpen_ && devicesChanged && config_.outputDeviceId.empty()) {
                // Following the Windows default: move if it changed.
                const std::string defaultId = defaultDeviceId(state().devices);
                if (!defaultId.empty() && defaultId != openedDefaultId_) openNow();
            }
        }

        const AudioStatus status = backend_->status();
        if (wantOpen_ && (status == AudioStatus::DeviceLost || status == AudioStatus::Failed)) {
            if (state().status != status) {
                publish([&](AudioServiceState& s) {
                    s.status = status;
                    s.stream = {};
                    s.message = status == AudioStatus::DeviceLost ? "The audio device was disconnected. Waiting for it to return."
                                                                  : (backend_->lastError().empty() ? "Audio stopped unexpectedly." : backend_->lastError());
                });
            }
            const auto now = std::chrono::steady_clock::now();
            if (devicesChanged || now >= nextRetry) {
                nextRetry = now + kRetryInterval;
                openNow();
            }
        }

        if (offlineRequest) {
            publish([](AudioServiceState& s) {
                s.offlineTestRunning = true;
                s.offlineTestResult.clear();
            });
            const ClickReport report = runOfflineMetronomeTest(kOfflineTestRate, kOfflineTestBpm, kOfflineTestSeconds, *offlineRequest);
            publish([&](AudioServiceState& s) {
                s.offlineTestRunning = false;
                s.offlineTestResult = report.summary;
            });
        }
    }

    backend_->stop();
    backend_.reset(); // COM objects must go before this thread leaves COM.
    CoUninitialize();
}

} // namespace ldaw::engine
