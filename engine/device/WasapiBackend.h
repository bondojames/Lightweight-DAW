// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/device/IAudioBackend.h"

#include <memory>

namespace ldaw::engine {

// Windows WASAPI output, shared mode and event-driven. Call all methods from one
// thread that has COM initialized for the multithreaded apartment.
class WasapiBackend final : public IAudioBackend {
public:
    WasapiBackend();
    ~WasapiBackend() override;

    const char* name() const override { return "WASAPI shared"; }
    std::vector<AudioDeviceInfo> enumerateOutputs() override;
    bool start(const AudioStreamConfig& config, IAudioProcessor& processor, AudioTelemetry& telemetry) override;
    void stop() override;
    AudioStatus status() const override;
    AudioStreamInfo streamInfo() const override;
    std::string lastError() const override;
    bool takeDevicesChanged() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ldaw::engine
