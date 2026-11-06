// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/device/AudioStatus.h"
#include "engine/graph/IAudioProcessor.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ldaw::engine {

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    std::uint32_t inputChannels = 0;
    std::uint32_t outputChannels = 0;
};

struct AudioStreamConfig {
    std::string outputDeviceId;
    std::string inputDeviceId;
    double sampleRate = 48000.0;
    std::uint32_t requestedFrames = 256;
};

// Driver-neutral audio device access. WASAPI arrives in Phase 01, ASIO before v1.0.
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;
    virtual const char* name() const = 0;
    virtual std::vector<AudioDeviceInfo> enumerateDevices() = 0;
    virtual bool start(const AudioStreamConfig& config, IAudioProcessor& processor) = 0;
    virtual void stop() = 0;
    virtual AudioStatus status() const = 0;
    virtual std::uint32_t negotiatedFrames() const = 0; // What the driver gave, not what was asked.
    virtual double negotiatedSampleRate() const = 0;
};

} // namespace ldaw::engine
