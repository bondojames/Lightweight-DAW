// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "engine/AudioEngine.h"
#include "engine/graph/ArrangementSnapshot.h"
#include "engine/media/MediaRuntime.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ldaw::engine {

struct ImportResult {
    std::uint64_t token = 0;
    std::filesystem::path path;
    bool ok = false;
    std::string error;
    double sourceSampleRate = 0.0;
    std::uint32_t channels = 0;
    std::int64_t framesAtSourceRate = 0;
    std::string decoderName;
};

// Decodes files on its own worker thread and keeps the ready-to-play media for the current
// sample rate. A rate change re-prepares everything in the background from the sources.
class MediaManager {
public:
    MediaManager(std::filesystem::path cacheDirectory, std::function<void()> onChanged);
    ~MediaManager();
    MediaManager(const MediaManager&) = delete;
    MediaManager& operator=(const MediaManager&) = delete;

    void requestImport(std::uint64_t token, const std::filesystem::path& path);
    std::vector<ImportResult> takeResults();

    void setSampleRate(double sampleRate);
    double sampleRate() const;

    // Ready media at the current rate; if not ready, queues it and returns null. Thread-safe.
    std::shared_ptr<MediaRuntime> runtimeFor(const std::filesystem::path& path);
    std::shared_ptr<const WaveformPeaks> peaksFor(const std::filesystem::path& path) const;

    std::size_t pendingJobs() const;
    std::string currentJobName() const;
    float currentJobProgress() const { return progress_.fraction.load(std::memory_order_relaxed); }
    void cancelAll();

private:
    struct Job {
        std::uint64_t token = 0; // 0 for a background re-prepare with no one waiting.
        std::filesystem::path path;
    };

    void threadMain();

    std::filesystem::path cacheDirectory_;
    std::function<void()> onChanged_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> jobs_;
    std::map<std::filesystem::path, std::shared_ptr<MediaRuntime>> ready_;
    std::map<std::filesystem::path, std::shared_ptr<const WaveformPeaks>> peaks_;
    std::map<std::filesystem::path, std::string> failed_;
    std::vector<ImportResult> results_;
    std::filesystem::path currentJob_;
    double sampleRate_ = 48000.0;
    bool quit_ = false;
    ImportProgress progress_;
    std::thread thread_;
};

// Loads audio ahead of the playhead on its own thread, and frees what's no longer near it,
// so memory stays bounded however long the song is. The audio thread is never involved.
class DiskStreamer {
public:
    static constexpr double kLookaheadSeconds = 4.0;
    static constexpr std::size_t kMaxRetired = 4096;

    explicit DiskStreamer(AudioEngine& engine);
    ~DiskStreamer();
    DiskStreamer(const DiskStreamer&) = delete;
    DiskStreamer& operator=(const DiskStreamer&) = delete;

    void setSnapshot(std::shared_ptr<const ArrangementSnapshot> snapshot);
    // Call after posting Play or Seek, so the load starts within milliseconds.
    void nudge();
    std::uint64_t loadedBlocks() const { return loaded_.load(std::memory_order_relaxed); }
    std::uint64_t readFailures() const { return failures_.load(std::memory_order_relaxed); }

private:
    struct Retired {
        const float* data = nullptr;
        std::uint64_t safeAfter = 0;
    };

    void threadMain();
    void updateFor(const ArrangementSnapshot& snapshot, std::int64_t position);
    void freeRetired();

    AudioEngine& engine_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::shared_ptr<const ArrangementSnapshot> snapshot_;
    bool quit_ = false;
    std::chrono::steady_clock::time_point fastUntil_{};
    std::vector<Retired> retired_;
    std::map<std::pair<const MediaRuntime*, std::int64_t>, std::shared_ptr<MediaRuntime>> held_;
    std::atomic<std::uint64_t> loaded_{0};
    std::atomic<std::uint64_t> failures_{0};
    std::thread thread_;
};

} // namespace ldaw::engine
