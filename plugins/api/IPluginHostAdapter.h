// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/graph/IAudioProcessor.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ldaw::plugins {

struct PluginDescriptor {
    std::string format; // e.g. "VST3"
    std::string uid;
    std::string name;
    std::string vendor;
    std::string version;
    bool isInstrument = false;
};

class IPluginInstance {
public:
    virtual ~IPluginInstance() = default;
    virtual engine::IAudioProcessor& processor() = 0;
    virtual std::vector<std::uint8_t> saveState() = 0;
    virtual bool restoreState(const std::vector<std::uint8_t>& state) = 0;
};

// Format-neutral hosting; VST3 arrives in Phase 09. Instances are created on the
// UI thread because VST3 expects it (ADR-008).
class IPluginHostAdapter {
public:
    virtual ~IPluginHostAdapter() = default;
    virtual const char* formatName() const = 0;
    virtual std::unique_ptr<IPluginInstance> instantiate(const PluginDescriptor& descriptor) = 0;
};

} // namespace ldaw::plugins
