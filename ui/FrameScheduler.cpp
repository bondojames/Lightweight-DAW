// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/FrameScheduler.h"

#include <algorithm>

namespace ldaw {

FrameScheduler::Mode FrameScheduler::mode(double now) const {
    if (minimized_ && !hasSecondary_) return Mode::Hidden;
    if (occluded_ && !hasSecondary_) return Mode::Occluded;
    if (minimized_) return Mode::Idle; // Only detached panels are showing.
    if ((now - lastActivity_) <= config_.activeHoldSeconds) return Mode::Active;
    return animating_ ? Mode::Animating : Mode::Idle;
}

double FrameScheduler::intervalFor(Mode mode) const {
    switch (mode) {
    case Mode::Active: return 1.0 / std::max(1.0, config_.activeHz);
    case Mode::Animating: return 1.0 / std::max(1.0, config_.animatingHz);
    case Mode::Idle: return 1.0 / std::max(0.1, config_.idleHz);
    case Mode::Occluded: return 1.0 / std::max(0.1, config_.occludedProbeHz);
    case Mode::Hidden: break;
    }
    return -1.0;
}

bool FrameScheduler::shouldRender(double now) const {
    const Mode current = mode(now);
    return current != Mode::Hidden && (now - lastFrame_) >= intervalFor(current);
}

double FrameScheduler::secondsUntilNextFrame(double now) const {
    const Mode current = mode(now);
    if (current == Mode::Hidden) return -1.0;
    return std::max(0.0, intervalFor(current) - (now - lastFrame_));
}

} // namespace ldaw
