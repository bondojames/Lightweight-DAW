// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace ldaw {

// Decides when the UI should draw. It holds no clock or window: callers pass time
// in seconds, so the policy is unit tested without a GPU. It affects UI drawing
// only and must never become a timing source for audio.
class FrameScheduler {
public:
    enum class Mode { Active, Animating, Idle, Occluded, Hidden };

    struct Config {
        double activeHz = 60.0;
        double idleHz = 10.0;
        double animatingHz = 30.0; // The playhead moves, so draw steadily, but only this fast.
        double occludedProbeHz = 2.0;
        double activeHoldSeconds = 0.5; // ImGui needs several frames to settle after input.
    };

    void setConfig(const Config& config) { config_ = config; }
    const Config& config() const { return config_; }

    void noteActivity(double now) { lastActivity_ = now; }
    void setMinimized(bool minimized) { minimized_ = minimized; }
    void setOccluded(bool occluded) { occluded_ = occluded; }
    void setAnimating(bool animating) { animating_ = animating; }
    // Detached panels stay visible when the main window is minimized or covered.
    void setHasSecondaryViewports(bool has) { hasSecondary_ = has; }

    Mode mode(double now) const;
    bool shouldRender(double now) const;
    void markRendered(double now) { lastFrame_ = now; }

    // Seconds to sleep before the next frame; negative means no frame is due at
    // all and the caller should wait only for messages.
    double secondsUntilNextFrame(double now) const;

private:
    double intervalFor(Mode mode) const;

    Config config_{};
    double lastActivity_ = -1.0e12;
    double lastFrame_ = -1.0e12;
    bool minimized_ = false;
    bool occluded_ = false;
    bool animating_ = false;
    bool hasSecondary_ = false;
};

} // namespace ldaw
