// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/layout/Geometry.h"
#include "tests/unit/test_framework.h"

using namespace ldaw;

static const std::vector<IntRect> kTwoMonitors{{0, 0, 1920, 1080}, {1920, 0, 2560, 1440}};
static const std::vector<IntRect> kOneMonitor{{0, 0, 1920, 1080}};

TEST_CASE(visible_window_is_untouched) {
    const IntRect r{2000, 100, 800, 600};
    CHECK(rescueRect(r, kTwoMonitors) == r);
}

TEST_CASE(window_on_unplugged_monitor_comes_back) {
    const IntRect r{2000, 100, 800, 600};
    const IntRect fixed = rescueRect(r, kOneMonitor);
    CHECK(isGrabbable(fixed, kOneMonitor));
    CHECK(fixed.x + fixed.w <= 1920);
    CHECK(fixed.w == 800);
}

TEST_CASE(window_larger_than_monitor_is_shrunk) {
    const IntRect fixed = rescueRect({5000, 5000, 4000, 3000}, kOneMonitor);
    CHECK(fixed == (IntRect{0, 0, 1920, 1080}));
}

TEST_CASE(title_bar_above_screen_is_rescued) {
    const IntRect r{100, -500, 800, 600}; // Body visible, title bar unreachable.
    CHECK(!isGrabbable(r, kOneMonitor));
    CHECK(rescueRect(r, kOneMonitor).y == 0);
}

TEST_CASE(negative_coordinate_monitor_is_valid) {
    const std::vector<IntRect> left{{-1920, 0, 1920, 1080}, {0, 0, 1920, 1080}};
    const IntRect r{-1800, 50, 600, 400};
    CHECK(rescueRect(r, left) == r);
}

TEST_CASE(no_monitors_returns_input) {
    const IntRect r{1, 2, 3, 4};
    CHECK(rescueRect(r, {}) == r);
}

TEST_CASE(extreme_coordinates_do_not_overflow) {
    const IntRect fixed = rescueRect({2147483000, 2147483000, 2000, 2000}, kOneMonitor);
    CHECK(isGrabbable(fixed, kOneMonitor));
}
