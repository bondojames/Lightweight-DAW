// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/media/MediaRuntime.h"

#include "engine/runtime/RtGuard.h"

#include <algorithm>

namespace ldaw::engine {
namespace {

constexpr float kEdgeFadeFrames = 64.0f; // About 1.3 ms at 48 kHz.

} // namespace

MediaRuntime::MediaRuntime(PreparedMedia media) : media_(std::move(media)) {
    blockCount_ = (media_.frames + kBlockFrames - 1) / kBlockFrames;
    blocks_ = std::make_unique<std::atomic<const float*>[]>(static_cast<std::size_t>(blockCount_));
    for (std::int64_t i = 0; i < blockCount_; ++i) blocks_[i].store(nullptr, std::memory_order_relaxed);
    _wfopen_s(&file_, media_.cachePath.c_str(), L"rb");
}

MediaRuntime::~MediaRuntime() {
    for (std::int64_t i = 0; i < blockCount_; ++i) delete[] blocks_[i].load(std::memory_order_relaxed);
    if (file_) std::fclose(file_);
}

std::uint32_t MediaRuntime::readInto(std::int64_t frame, float* left, float* right, std::uint32_t count, float gain, std::int64_t fadeInLeft, std::int64_t fadeOutLeft) const noexcept {
    std::uint32_t missing = 0;
    const std::uint32_t channels = media_.channels;
    std::uint32_t done = 0;
    while (done < count) {
        const std::int64_t position = frame + done;
        if (position < 0 || position >= media_.frames) {
            ++done;
            continue;
        }
        const std::int64_t block = position / kBlockFrames;
        const std::int64_t offset = position % kBlockFrames;
        const auto run = static_cast<std::uint32_t>(std::min<std::int64_t>({kBlockFrames - offset, media_.frames - position, static_cast<std::int64_t>(count - done)}));
        const float* data = blocks_[block].load(std::memory_order_acquire);
        if (!data) {
            missing += run;
            done += run;
            continue;
        }
        const float* source = data + offset * channels;
        for (std::uint32_t i = 0; i < run; ++i) {
            const std::int64_t index = static_cast<std::int64_t>(done) + i;
            // Each clip edge fades very briefly, so cutting into a waveform never clicks.
            const float in = static_cast<float>(fadeInLeft + index + 1) / kEdgeFadeFrames;
            const float out = static_cast<float>(fadeOutLeft - index) / kEdgeFadeFrames;
            const float g = gain * std::clamp(std::min(in, out), 0.0f, 1.0f);
            if (channels == 1) {
                const float s = source[i] * g;
                left[index] += s;
                right[index] += s;
            } else {
                left[index] += source[i * 2] * g;
                right[index] += source[i * 2 + 1] * g;
            }
        }
        done += run;
    }
    return missing;
}

bool MediaRuntime::load(std::int64_t block) {
    if (block < 0 || block >= blockCount_ || isLoaded(block) || !file_) return block >= 0 && block < blockCount_ && isLoaded(block);
    if (g_inRealtimeSection) g_realtimeDiskReads.fetch_add(1, std::memory_order_relaxed);
    const std::int64_t first = block * kBlockFrames;
    const std::int64_t frames = std::min(kBlockFrames, media_.frames - first);
    const std::size_t samples = static_cast<std::size_t>(frames) * media_.channels;
    auto* data = new float[samples];
    if (_fseeki64(file_, first * media_.channels * static_cast<std::int64_t>(sizeof(float)), SEEK_SET) != 0 || std::fread(data, sizeof(float), samples, file_) != samples) {
        delete[] data;
        return false;
    }
    blocks_[block].store(data, std::memory_order_release);
    return true;
}

bool MediaRuntime::loadAll() {
    for (std::int64_t i = 0; i < blockCount_; ++i) {
        if (!load(i)) return false;
    }
    return true;
}

const float* MediaRuntime::evict(std::int64_t block) { return blocks_[block].exchange(nullptr, std::memory_order_acq_rel); }

} // namespace ldaw::engine
