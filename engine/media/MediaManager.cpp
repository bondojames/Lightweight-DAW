// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine/media/MediaManager.h"

#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <chrono>

namespace ldaw::engine {

MediaManager::MediaManager(std::filesystem::path cacheDirectory, std::function<void()> onChanged)
    : cacheDirectory_(std::move(cacheDirectory)), onChanged_(std::move(onChanged)) {
    thread_ = std::thread([this] { threadMain(); });
}

MediaManager::~MediaManager() {
    {
        const std::scoped_lock lock(mutex_);
        quit_ = true;
        progress_.cancel.store(true, std::memory_order_relaxed);
    }
    wake_.notify_all();
    thread_.join();
}

void MediaManager::requestImport(std::uint64_t token, const std::filesystem::path& path) {
    {
        const std::scoped_lock lock(mutex_);
        jobs_.push_back({token, path});
    }
    wake_.notify_all();
}

std::vector<ImportResult> MediaManager::takeResults() {
    const std::scoped_lock lock(mutex_);
    std::vector<ImportResult> out;
    out.swap(results_);
    return out;
}

void MediaManager::setSampleRate(double sampleRate) {
    {
        const std::scoped_lock lock(mutex_);
        if (sampleRate == sampleRate_) return;
        sampleRate_ = sampleRate;
        ready_.clear(); // Snapshots keep the old ones alive for as long as they're in use.
        failed_.clear();
    }
    wake_.notify_all();
}

double MediaManager::sampleRate() const {
    const std::scoped_lock lock(mutex_);
    return sampleRate_;
}

std::shared_ptr<MediaRuntime> MediaManager::runtimeFor(const std::filesystem::path& path) {
    bool queued = false;
    std::shared_ptr<MediaRuntime> found;
    {
        const std::scoped_lock lock(mutex_);
        const auto it = ready_.find(path);
        if (it != ready_.end()) {
            found = it->second;
        } else if (failed_.count(path) == 0) {
            const bool waiting = std::any_of(jobs_.begin(), jobs_.end(), [&](const Job& job) { return job.path == path; });
            if (!waiting && currentJob_ != path) {
                jobs_.push_back({0, path});
                queued = true;
            }
        }
    }
    if (queued) wake_.notify_all();
    return found;
}

std::shared_ptr<const WaveformPeaks> MediaManager::peaksFor(const std::filesystem::path& path) const {
    const std::scoped_lock lock(mutex_);
    const auto it = peaks_.find(path);
    return it == peaks_.end() ? nullptr : it->second;
}

std::size_t MediaManager::pendingJobs() const {
    const std::scoped_lock lock(mutex_);
    return jobs_.size() + (currentJob_.empty() ? 0u : 1u);
}

std::string MediaManager::currentJobName() const {
    const std::scoped_lock lock(mutex_);
    const std::u8string name = currentJob_.filename().u8string();
    return std::string(name.begin(), name.end());
}

void MediaManager::cancelAll() {
    const std::scoped_lock lock(mutex_);
    jobs_.clear();
    progress_.cancel.store(true, std::memory_order_relaxed);
}

void MediaManager::threadMain() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    for (;;) {
        Job job;
        double rate = 0.0;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return quit_ || !jobs_.empty(); });
            if (quit_) break;
            job = jobs_.front();
            jobs_.pop_front();
            rate = sampleRate_;
            currentJob_ = job.path;
            progress_.fraction.store(0.0f, std::memory_order_relaxed);
            progress_.cancel.store(false, std::memory_order_relaxed);
        }

        PreparedMedia prepared;
        std::string error;
        const bool ok = prepareMedia(job.path, rate, cacheDirectory_, prepared, error, &progress_);

        {
            const std::scoped_lock lock(mutex_);
            currentJob_.clear();
            // If the rate changed while this ran, this result is stale: do it again.
            if (ok && rate == sampleRate_) {
                ready_[job.path] = std::make_shared<MediaRuntime>(prepared);
                peaks_[job.path] = prepared.peaks;
            } else if (ok) {
                jobs_.push_back({0, job.path});
            } else {
                failed_[job.path] = error;
            }
            if (job.token != 0) {
                ImportResult result;
                result.token = job.token;
                result.path = job.path;
                result.ok = ok;
                result.error = error;
                result.sourceSampleRate = prepared.sourceSampleRate;
                result.channels = prepared.channels;
                result.framesAtSourceRate = ok ? static_cast<std::int64_t>(static_cast<double>(prepared.frames) * prepared.sourceSampleRate / prepared.sampleRate + 0.5) : 0;
                result.decoderName = prepared.decoderName;
                results_.push_back(std::move(result));
            }
        }
        if (onChanged_) onChanged_();
    }
    CoUninitialize();
}

DiskStreamer::DiskStreamer(AudioEngine& engine) : engine_(engine) {
    engine_.setPreloadGate(true);
    thread_ = std::thread([this] { threadMain(); });
}

DiskStreamer::~DiskStreamer() {
    {
        const std::scoped_lock lock(mutex_);
        quit_ = true;
    }
    wake_.notify_all();
    thread_.join();
    engine_.setPreloadGate(false);
    // The audio stream is stopped by now, so nothing can still be reading these.
    for (const Retired& retired : retired_) delete[] retired.data;
}

void DiskStreamer::setSnapshot(std::shared_ptr<const ArrangementSnapshot> snapshot) {
    {
        const std::scoped_lock lock(mutex_);
        snapshot_ = std::move(snapshot);
    }
    wake_.notify_all();
}

void DiskStreamer::nudge() {
    {
        const std::scoped_lock lock(mutex_);
        fastUntil_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    }
    wake_.notify_all();
}

void DiskStreamer::freeRetired() {
    const std::uint64_t now = engine_.processCount();
    std::erase_if(retired_, [&](const Retired& retired) {
        if (retired.safeAfter > now) return false;
        delete[] retired.data;
        return true;
    });
}

void DiskStreamer::updateFor(const ArrangementSnapshot& snapshot, std::int64_t position) {
    const auto lookahead = static_cast<std::int64_t>(snapshot.sampleRate * kLookaheadSeconds);
    const auto needed = neededBlocks(snapshot, position, lookahead);

    // Nearest first would be better for a huge jump, but the set is small and sorted by block.
    std::map<const MediaRuntime*, std::shared_ptr<MediaRuntime>> owners;
    for (const auto& media : snapshot.media) owners[media.get()] = media;

    for (const auto& key : needed) {
        if (held_.count(key) != 0) continue;
        const auto owner = owners.find(key.first);
        if (owner == owners.end()) continue;
        if (owner->second->load(key.second)) {
            held_[key] = owner->second;
            loaded_.fetch_add(1, std::memory_order_relaxed);
        } else {
            failures_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    for (auto it = held_.begin(); it != held_.end();) {
        if (needed.count(it->first) != 0) {
            ++it;
            continue;
        }
        if (retired_.size() >= kMaxRetired) break; // The stream is stopped: hold on for now.
        if (const float* data = it->second->evict(it->first.second)) {
            // A callback may still be reading this until the count has moved on by two.
            retired_.push_back({data, engine_.processCount() + 2});
        }
        it = held_.erase(it);
    }
}

void DiskStreamer::threadMain() {
    for (;;) {
        std::shared_ptr<const ArrangementSnapshot> snapshot;
        {
            std::unique_lock lock(mutex_);
            const EngineState state = engine_.state();
            const bool busy = state.playing || engine_.pendingPreloadFrame() != AudioEngine::kNoPendingPreload;
            const bool fast = std::chrono::steady_clock::now() < fastUntil_;
            wake_.wait_for(lock, std::chrono::milliseconds(fast ? 3 : (busy ? 20 : 100)));
            if (quit_) break;
            snapshot = snapshot_;
        }
        if (snapshot) {
            const std::int64_t pending = engine_.pendingPreloadFrame();
            const std::int64_t position = pending != AudioEngine::kNoPendingPreload ? pending : engine_.state().positionFrames;
            updateFor(*snapshot, position);
            if (pending != AudioEngine::kNoPendingPreload) engine_.markPreloaded(pending);
        }
        freeRetired();
    }
}

} // namespace ldaw::engine
