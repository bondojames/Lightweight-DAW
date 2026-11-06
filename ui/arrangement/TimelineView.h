// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model/Project.h"

#include <algorithm>
#include <cmath>

namespace ldaw::ui {

// Maps between timeline time and screen pixels. Pure, so it's testable, and it stays
// monotonic at any song length. It holds view state only; nothing here is project data.
class TimelineView {
public:
    static constexpr double kMinPixelsPerSecond = 0.5;
    static constexpr double kMaxPixelsPerSecond = 4000.0;

    double pixelsPerSecond() const { return pixelsPerSecond_; }
    double scrollSeconds() const { return scrollSeconds_; }

    void setScroll(double seconds) { scrollSeconds_ = std::max(0.0, seconds); }

    double timeToPixel(model::Time time) const { return (model::timeToSeconds(time) - scrollSeconds_) * pixelsPerSecond_; }

    model::Time pixelToTime(double pixel) const {
        return model::secondsToTime(std::max(0.0, pixel / pixelsPerSecond_ + scrollSeconds_));
    }

    // Keeps whatever is under the given pixel in place.
    void zoomAround(double pixel, double factor) {
        const double anchor = pixel / pixelsPerSecond_ + scrollSeconds_;
        pixelsPerSecond_ = std::clamp(pixelsPerSecond_ * factor, kMinPixelsPerSecond, kMaxPixelsPerSecond);
        setScroll(anchor - pixel / pixelsPerSecond_);
    }

private:
    double pixelsPerSecond_ = 40.0;
    double scrollSeconds_ = 0.0;
};

// Rounds a time to the nearest beat.
inline model::Time snapToBeat(model::Time time, double bpm) {
    if (bpm <= 0.0) return time;
    const double beatSeconds = 60.0 / bpm;
    return model::secondsToTime(std::round(model::timeToSeconds(time) / beatSeconds) * beatSeconds);
}

// A readable bar spacing at this zoom: 1, 2, 4, 8... bars, at least minPixels apart.
inline int barsPerLabel(double pixelsPerSecond, double bpm, int beatsPerBar, double minPixels) {
    const double barPixels = pixelsPerSecond * 60.0 / bpm * beatsPerBar;
    int bars = 1;
    while (barPixels * bars < minPixels && bars < (1 << 20)) bars *= 2;
    return bars;
}

} // namespace ldaw::ui
