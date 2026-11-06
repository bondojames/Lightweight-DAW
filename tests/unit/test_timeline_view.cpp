// SPDX-License-Identifier: GPL-3.0-or-later
#include "tests/unit/test_framework.h"
#include "ui/arrangement/TimelineView.h"

#include <cmath>

using namespace ldaw::ui;
using namespace ldaw::model;

TEST_CASE(time_to_pixel_is_monotonic_past_two_hours_at_96k) {
    TimelineView view;
    view.zoomAround(0.0, 1000.0); // Well zoomed in, where rounding would show first.
    const std::int64_t twoHours = std::int64_t{2} * 3600 * 96000;
    double last = view.timeToPixel(framesToTime(twoHours - 1000, 96000.0));
    for (std::int64_t frame = twoHours - 999; frame < twoHours + 1000; ++frame) {
        const double pixel = view.timeToPixel(framesToTime(frame, 96000.0));
        CHECK(pixel > last);
        last = pixel;
    }
}

TEST_CASE(pixel_round_trip_is_close) {
    TimelineView view;
    view.setScroll(1234.5);
    const Time time = secondsToTime(1300.0);
    const Time back = view.pixelToTime(view.timeToPixel(time));
    CHECK(std::abs(timeToSeconds(back) - 1300.0) < 1e-6);
}

TEST_CASE(pixel_before_the_start_clamps_to_zero) {
    TimelineView view;
    CHECK(view.pixelToTime(-500.0) == 0);
}

TEST_CASE(zoom_keeps_the_point_under_the_mouse) {
    TimelineView view;
    view.setScroll(10.0);
    const double pixel = 300.0;
    const Time before = view.pixelToTime(pixel);
    view.zoomAround(pixel, 2.5);
    CHECK(std::abs(timeToSeconds(view.pixelToTime(pixel)) - timeToSeconds(before)) < 1e-6);
}

TEST_CASE(zoom_is_limited) {
    TimelineView view;
    view.zoomAround(0.0, 1.0e12);
    CHECK(view.pixelsPerSecond() == TimelineView::kMaxPixelsPerSecond);
    view.zoomAround(0.0, 1.0e-12);
    CHECK(view.pixelsPerSecond() == TimelineView::kMinPixelsPerSecond);
}

TEST_CASE(scroll_cannot_go_before_the_start) {
    TimelineView view;
    view.setScroll(-5.0);
    CHECK(view.scrollSeconds() == 0.0);
    view.zoomAround(0.0, 0.5);
    CHECK(view.scrollSeconds() == 0.0);
}

TEST_CASE(snap_goes_to_the_nearest_beat) {
    CHECK(snapToBeat(secondsToTime(1.2), 120.0) == secondsToTime(1.0));
    CHECK(snapToBeat(secondsToTime(1.3), 120.0) == secondsToTime(1.5));
    CHECK(snapToBeat(secondsToTime(1.3), 0.0) == secondsToTime(1.3));
}

TEST_CASE(bar_labels_thin_out_when_zoomed_out) {
    CHECK(barsPerLabel(100.0, 120.0, 4, 50.0) == 1);
    CHECK(barsPerLabel(1.0, 120.0, 4, 50.0) == 32);
}
