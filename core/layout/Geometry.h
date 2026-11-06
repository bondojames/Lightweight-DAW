// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vector>

namespace ldaw {

struct IntRect {
    int x = 0, y = 0, w = 0, h = 0;
    bool operator==(const IntRect&) const = default;
};

// True if enough of the rect's top edge lies on a monitor for the user to grab it.
bool isGrabbable(const IntRect& rect, const std::vector<IntRect>& monitors, int minVisiblePixels = 48);

// Returns rect unchanged if grabbable, otherwise moved fully onto the monitor it
// overlaps most (or the first monitor) and shrunk to fit. With no monitors listed
// the rect is returned as is.
IntRect rescueRect(const IntRect& rect, const std::vector<IntRect>& monitors, int minVisiblePixels = 48);

} // namespace ldaw
