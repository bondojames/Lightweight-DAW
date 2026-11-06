// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/device/AudioStatus.h"
#include "engine/graph/IAudioProcessor.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ldaw::engine {

class AudioTelemetry;

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    bool isDefault = false;
};

struct AudioStreamConfig {
    std::string outputDeviceId;      // Empty means the Windows default device.
    double sampleRate = 0.0;         // 0 means whatever the device is running at.
    std::uint32_t requestedFrames = 256;
    // Test only: burns this long inside the callback at this interval, to prove overruns are detected.
    double testStallMs = 0.0;
    double testStallEverySeconds = 0.0;
};

// What the driver actually gave us, which may differ from what was asked for.
struct AudioStreamInfo {
    std::string deviceName;
    std::string deviceId;
    std::string backendName;
    std::string deviceFormat;
    double sampleRate = 0.0;
    std::uint32_t periodFrames = 0;
    std::uint32_t bufferFrames = 0;
    std::uint32_t channels = 0;
    double outputLatencySeconds = 0.0;
    bool lowLatencyMode = false;  // The period was negotiated, not the 10 ms Windows default.
    bool systemResampling = false; // Windows is converting to the device's rate.
    bool realtimePriority = false; // MMCSS "Pro Audio" was granted.
};

// Driver-neutral audio device access. WASAPI is the first; ASIO comes before v1.0.
class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;
    virtual const char* name() const = 0;
    virtual std::vector<AudioDeviceInfo> enumerateOutputs() = 0;
    // Blocks until the stream is running or has failed; never call it from the UI thread.
    virtual bool start(const AudioStreamConfig& config, IAudioProcessor& processor, AudioTelemetry& telemetry) = 0;
    virtual void stop() = 0;
    virtual AudioStatus status() const = 0;
    virtual AudioStreamInfo streamInfo() const = 0;
    virtual std::string lastError() const = 0;
    // True once after devices were added, removed, or the default changed.
    virtual bool takeDevicesChanged() = 0;
};

} // namespace ldaw::engine
