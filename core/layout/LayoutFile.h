// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/layout/Geometry.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ldaw {

// A saved workspace: stable panel IDs with their visibility, plus the opaque text
// Dear ImGui produces for docking. Panel state is written as ';' comment lines,
// which ImGui's own ini reader skips.
struct LayoutFile {
    static constexpr int kCurrentVersion = 1;
    std::map<std::string, bool> panels;
    std::string imguiIni;
};

std::string serializeLayout(const LayoutFile& layout);

// nullopt means the file is corrupt or from an unknown version; the caller
// falls back to the factory layout and tells the user.
std::optional<LayoutFile> parseLayout(const std::string& text);

// Moves floating windows in ImGui ini text back onto a connected monitor, so a
// monitor removed since the last run cannot strand a panel off-screen.
std::string rescueFloatingWindows(const std::string& imguiIni, const std::vector<IntRect>& monitors);

} // namespace ldaw
