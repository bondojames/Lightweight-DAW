// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace ldaw::engine {

// Phase 00 has no audio engine, and the UI says so rather than simulating one.
enum class AudioStatus { NotInitialized, Running, Stopped, DeviceLost, Failed };

constexpr const char* audioStatusText(AudioStatus status) {
    switch (status) {
    case AudioStatus::NotInitialized: return "not initialized";
    case AudioStatus::Running: return "running";
    case AudioStatus::Stopped: return "stopped";
    case AudioStatus::DeviceLost: return "device lost";
    case AudioStatus::Failed: return "failed";
    }
    return "unknown";
}

} // namespace ldaw::engine
