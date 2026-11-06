// SPDX-License-Identifier: GPL-3.0-or-later
#include "tests/unit/test_framework.h"
#include "ui/FrameScheduler.h"

#include <cmath>

using namespace ldaw;
using Mode = FrameScheduler::Mode;

static int framesIn(FrameScheduler& s, double start, double seconds) {
    int frames = 0;
    for (double t = start; t < start + seconds; t += 0.001) {
        if (s.shouldRender(t)) { s.markRendered(t); ++frames; }
    }
    return frames;
}

TEST_CASE(idle_rate_is_bounded) {
    FrameScheduler s;
    const int frames = framesIn(s, 100.0, 10.0);
    CHECK(frames >= 95 && frames <= 101); // 10 Hz, not as fast as the loop runs.
}

TEST_CASE(activity_raises_rate_then_decays) {
    FrameScheduler s;
    s.noteActivity(100.0);
    CHECK(s.mode(100.1) == Mode::Active);
    const int frames = framesIn(s, 100.0, 0.5);
    CHECK(frames >= 28 && frames <= 31);
    CHECK(s.mode(100.6) == Mode::Idle);
}

TEST_CASE(minimized_never_renders) {
    FrameScheduler s;
    s.setMinimized(true);
    s.noteActivity(100.0);
    CHECK(s.mode(100.0) == Mode::Hidden);
    CHECK(framesIn(s, 100.0, 5.0) == 0);
    CHECK(s.secondsUntilNextFrame(100.0) < 0.0);
}

TEST_CASE(minimized_with_detached_panel_keeps_drawing_at_idle) {
    FrameScheduler s;
    s.setMinimized(true);
    s.setHasSecondaryViewports(true);
    s.noteActivity(100.0);
    CHECK(s.mode(100.0) == Mode::Idle);
    CHECK(framesIn(s, 100.0, 1.0) > 0);
}

TEST_CASE(occluded_only_probes) {
    FrameScheduler s;
    s.setOccluded(true);
    s.noteActivity(100.0);
    CHECK(s.mode(100.0) == Mode::Occluded);
    const int frames = framesIn(s, 100.0, 5.0);
    CHECK(frames >= 9 && frames <= 11);
}

TEST_CASE(wait_never_negative_while_visible) {
    FrameScheduler s;
    s.markRendered(100.0);
    CHECK(std::abs(s.secondsUntilNextFrame(100.04) - 0.06) < 1e-9);
    CHECK(s.secondsUntilNextFrame(105.0) == 0.0);
}

TEST_CASE(configured_rates_are_used_and_zero_is_safe) {
    FrameScheduler s;
    FrameScheduler::Config c;
    c.idleHz = 0.0;
    c.activeHz = 30.0;
    s.setConfig(c);
    s.noteActivity(100.0);
    const int frames = framesIn(s, 100.0, 0.5);
    CHECK(frames >= 14 && frames <= 16);
    CHECK(framesIn(s, 200.0, 5.0) <= 1);
}

TEST_CASE(playing_draws_at_the_animation_rate_and_input_still_wins) {
    FrameScheduler s;
    s.setAnimating(true);
    CHECK(s.mode(100.0) == Mode::Animating);
    const int frames = framesIn(s, 100.0, 1.0);
    CHECK(frames >= 29 && frames <= 31);
    s.noteActivity(200.0);
    CHECK(s.mode(200.1) == Mode::Active);
    s.setMinimized(true);
    CHECK(s.mode(300.0) == Mode::Hidden);
}
