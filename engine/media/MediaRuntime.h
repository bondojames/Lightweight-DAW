// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/media/MediaImport.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace ldaw::engine {

// Counted by test programs: any disk read while this thread is in a RealtimeSection is a bug.
inline std::atomic<std::uint64_t> g_realtimeDiskReads{0};

// One media file, ready to play, split into blocks. A worker thread loads blocks ahead of
// the playhead and publishes each with an atomic pointer. The audio thread only ever reads
// those pointers, so it can't block on the disk: a block that isn't there yet plays as
// silence and is counted, which makes the failure visible instead of an audio stall.
class MediaRuntime {
public:
    static constexpr std::int64_t kBlockFrames = 65536;

    explicit MediaRuntime(PreparedMedia media);
    ~MediaRuntime();
    MediaRuntime(const MediaRuntime&) = delete;
    MediaRuntime& operator=(const MediaRuntime&) = delete;

    const PreparedMedia& media() const { return media_; }
    std::int64_t frames() const { return media_.frames; }
    std::uint32_t channels() const { return media_.channels; }
    std::int64_t blockCount() const { return blockCount_; }

    // Audio thread. Adds frames to the buffers and returns how many were unavailable.
    // Allocation-free, lock-free, and never touches the disk.
    std::uint32_t readInto(std::int64_t frame, float* left, float* right, std::uint32_t count, float gain, std::int64_t fadeInLeft, std::int64_t fadeOutLeft) const noexcept;

    // Worker thread only.
    bool isLoaded(std::int64_t block) const { return blocks_[block].load(std::memory_order_acquire) != nullptr; }
    bool load(std::int64_t block);
    bool loadAll();
    // The caller must not free the result until no audio callback can still be using it.
    const float* evict(std::int64_t block);

private:
    PreparedMedia media_;
    std::int64_t blockCount_ = 0;
    std::unique_ptr<std::atomic<const float*>[]> blocks_;
    std::FILE* file_ = nullptr;
};

} // namespace ldaw::engine
