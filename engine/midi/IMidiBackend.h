// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ldaw::engine {

struct MidiDeviceInfo {
    std::string id;
    std::string name;
};

struct MidiMessage {
    std::uint8_t data[3] = {0, 0, 0};
    std::uint8_t size = 0;
    std::int64_t sourceTimestampNs = 0;
};

// MIDI device access; arrives in Phase 05. Input is delivered on a driver thread.
class IMidiBackend {
public:
    using InputHandler = std::function<void(const MidiMessage&)>;
    virtual ~IMidiBackend() = default;
    virtual std::vector<MidiDeviceInfo> enumerateInputs() = 0;
    virtual std::vector<MidiDeviceInfo> enumerateOutputs() = 0;
    virtual bool openInput(const std::string& id, InputHandler handler) = 0;
    virtual void closeInput(const std::string& id) = 0;
    virtual bool openOutput(const std::string& id) = 0;
    virtual void closeOutput(const std::string& id) = 0;
    virtual bool send(const std::string& id, const MidiMessage& message) = 0;
};

} // namespace ldaw::engine
