// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/layout/Geometry.h"

#include <algorithm>
#include <cstdint>

namespace ldaw {
namespace {

std::int64_t overlapArea(const IntRect& a, const IntRect& b) {
    const std::int64_t w = std::min(std::int64_t{a.x} + a.w, std::int64_t{b.x} + b.w) - std::max(a.x, b.x);
    const std::int64_t h = std::min(std::int64_t{a.y} + a.h, std::int64_t{b.y} + b.h) - std::max(a.y, b.y);
    return (w > 0 && h > 0) ? w * h : 0;
}

} // namespace

bool isGrabbable(const IntRect& rect, const std::vector<IntRect>& monitors, int minVisiblePixels) {
    // The strip a user can drag: the top 24 pixels of the window.
    const IntRect strip{rect.x, rect.y, rect.w, std::min(24, rect.h)};
    const std::int64_t needed = std::int64_t{std::min(minVisiblePixels, rect.w)} * strip.h;
    for (const IntRect& monitor : monitors) {
        if (strip.h > 0 && overlapArea(strip, monitor) >= needed) return true;
    }
    return false;
}

IntRect rescueRect(const IntRect& rect, const std::vector<IntRect>& monitors, int minVisiblePixels) {
    if (monitors.empty() || isGrabbable(rect, monitors, minVisiblePixels)) return rect;

    const IntRect* target = &monitors.front();
    std::int64_t best = 0;
    for (const IntRect& monitor : monitors) {
        const std::int64_t area = overlapArea(rect, monitor);
        if (area > best) { best = area; target = &monitor; }
    }
    IntRect out = rect;
    out.w = std::min(rect.w, target->w);
    out.h = std::min(rect.h, target->h);
    out.x = std::clamp(rect.x, target->x, target->x + target->w - out.w);
    out.y = std::clamp(rect.y, target->y, target->y + target->h - out.h);
    return out;
}

} // namespace ldaw
