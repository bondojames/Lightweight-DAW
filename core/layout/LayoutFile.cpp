// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/layout/LayoutFile.h"

#include <cstdio>
#include <sstream>

namespace ldaw {
namespace {

constexpr const char* kHeaderPrefix = "; LightweightDAW layout v";
constexpr const char* kPanelsPrefix = "; panels=";
constexpr std::size_t kMaxFileBytes = 4u * 1024u * 1024u;

bool startsWith(const std::string& text, const char* prefix) { return text.rfind(prefix, 0) == 0; }

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

} // namespace

std::string serializeLayout(const LayoutFile& layout) {
    std::string out = std::string(kHeaderPrefix) + std::to_string(LayoutFile::kCurrentVersion) + "\n" + kPanelsPrefix;
    bool first = true;
    for (const auto& [id, visible] : layout.panels) {
        if (!first) out += ',';
        out += id + ":" + (visible ? "1" : "0");
        first = false;
    }
    out += "\n";
    out += layout.imguiIni;
    return out;
}

std::optional<LayoutFile> parseLayout(const std::string& text) {
    if (text.empty() || text.size() > kMaxFileBytes) return std::nullopt;
    if (text.find('\0') != std::string::npos) return std::nullopt;

    const std::vector<std::string> lines = splitLines(text);
    if (lines.size() < 2) return std::nullopt;
    if (lines[0] != std::string(kHeaderPrefix) + std::to_string(LayoutFile::kCurrentVersion)) return std::nullopt;
    if (!startsWith(lines[1], kPanelsPrefix)) return std::nullopt;

    LayoutFile layout;
    std::istringstream panels(lines[1].substr(std::char_traits<char>::length(kPanelsPrefix)));
    std::string entry;
    while (std::getline(panels, entry, ',')) {
        const auto colon = entry.rfind(':');
        if (colon == std::string::npos || colon == 0 || colon + 2 != entry.size()) return std::nullopt;
        const char value = entry[colon + 1];
        if (value != '0' && value != '1') return std::nullopt;
        layout.panels[entry.substr(0, colon)] = (value == '1');
    }

    // Everything after the header must have ini shape, or ImGui would silently
    // discard it and the user would see panels vanish with no explanation.
    bool sawSection = false;
    for (std::size_t i = 2; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        if (!line.empty() && line[0] == ';') continue;
        if (!line.empty() && line[0] == '[') {
            if (line.back() != ']') return std::nullopt;
            sawSection = true;
        } else if (!line.empty() && !sawSection) {
            return std::nullopt;
        }
        layout.imguiIni += line;
        layout.imguiIni += '\n';
    }
    if (!sawSection) return std::nullopt;
    return layout;
}

std::string rescueFloatingWindows(const std::string& imguiIni, const std::vector<IntRect>& monitors) {
    std::vector<std::string> lines = splitLines(imguiIni);

    std::size_t i = 0;
    while (i < lines.size()) {
        if (!startsWith(lines[i], "[Window][")) { ++i; continue; }
        std::size_t end = i + 1;
        while (end < lines.size() && !(!lines[end].empty() && lines[end][0] == '[')) ++end;

        // ImGui stores Pos relative to ViewportPos; only windows that own a platform
        // window have a ViewportPos line, and docked ones follow their dock node.
        int viewX = 0, viewY = 0, posX = 0, posY = 0, sizeW = 400, sizeH = 300;
        std::size_t viewLine = 0;
        bool hasViewport = false, docked = false;
        for (std::size_t k = i + 1; k < end; ++k) {
            const char* s = lines[k].c_str();
            if (sscanf_s(s, "ViewportPos=%d,%d", &viewX, &viewY) == 2) { hasViewport = true; viewLine = k; }
            else if (sscanf_s(s, "Pos=%d,%d", &posX, &posY) == 2) {}
            else if (sscanf_s(s, "Size=%d,%d", &sizeW, &sizeH) == 2) {}
            else if (startsWith(lines[k], "DockId=")) docked = true;
        }
        if (hasViewport && !docked) {
            const IntRect current{viewX + posX, viewY + posY, sizeW, sizeH};
            const IntRect fixed = rescueRect(current, monitors);
            if (!(fixed == current)) {
                lines[viewLine] = "ViewportPos=" + std::to_string(fixed.x - posX) + "," + std::to_string(fixed.y - posY);
            }
        }
        i = end;
    }

    std::string out;
    for (const std::string& line : lines) { out += line; out += '\n'; }
    return out;
}

} // namespace ldaw
