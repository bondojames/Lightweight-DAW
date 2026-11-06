// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/layout/LayoutFile.h"
#include "tests/unit/test_framework.h"

using namespace ldaw;

static LayoutFile sample() {
    LayoutFile layout;
    layout.panels = {{"panel.library", true}, {"panel.mixer", false}};
    layout.imguiIni = "[Window][Library]\nPos=0,0\nSize=100,100\n\n[Docking][Data]\n";
    return layout;
}

TEST_CASE(layout_round_trip) {
    const auto parsed = parseLayout(serializeLayout(sample()));
    CHECK(parsed.has_value());
    CHECK(parsed->panels.at("panel.library"));
    CHECK(!parsed->panels.at("panel.mixer"));
    CHECK(parsed->imguiIni == sample().imguiIni);
}

TEST_CASE(layout_rejects_corrupt_input) {
    CHECK(!parseLayout("").has_value());
    CHECK(!parseLayout("garbage").has_value());
    std::string withNul = "; LightweightDAW layout v1\n; panels=\n[Window][A]\n";
    withNul[30] = '\0';
    CHECK(!parseLayout(withNul).has_value());
    CHECK(!parseLayout("; LightweightDAW layout v1\n; panels=a:9\n[Window][A]\n").has_value());
    CHECK(!parseLayout("; LightweightDAW layout v1\n; panels=a\n[Window][A]\n").has_value());
    CHECK(!parseLayout("; LightweightDAW layout v1\n; panels=a:1\nPos=1,1\n[Window][A]\n").has_value());
    CHECK(!parseLayout("; LightweightDAW layout v1\n; panels=a:1\n[Window][A\n").has_value());
    CHECK(!parseLayout("; LightweightDAW layout v1\n; panels=a:1\n").has_value());
    CHECK(!parseLayout(std::string(5 * 1024 * 1024, 'x')).has_value());
}

TEST_CASE(layout_rejects_unknown_version) {
    CHECK(!parseLayout("; LightweightDAW layout v999\n; panels=a:1\n[Window][A]\n").has_value());
}

TEST_CASE(layout_truncated_at_every_length_does_not_crash) {
    const std::string full = serializeLayout(sample());
    for (std::size_t n = 0; n < full.size(); ++n) (void)parseLayout(full.substr(0, n));
    CHECK(parseLayout(full).has_value());
}

TEST_CASE(floating_window_off_monitor_is_moved_back) {
    const std::string ini =
        "[Window][Mixer]\nViewportPos=3000,200\nViewportId=0x1234\nPos=0,0\nSize=800,600\n\n"
        "[Window][Docked]\nViewportPos=3000,200\nViewportId=0x1\nPos=0,0\nSize=8,6\nDockId=0x2,0\n\n"
        "[Window][Main]\nPos=10,10\nSize=100,100\n\n[Docking][Data]\n";
    const std::string out = rescueFloatingWindows(ini, {{0, 0, 1920, 1080}});
    CHECK(out.find("[Window][Mixer]\nViewportPos=1120,200\n") != std::string::npos);
    CHECK(out.find("[Window][Docked]\nViewportPos=3000,200\n") != std::string::npos);
    CHECK(out.find("[Window][Main]\nPos=10,10\n") != std::string::npos);
    CHECK(out.find("[Docking][Data]") != std::string::npos);
}

TEST_CASE(floating_window_still_visible_is_unchanged) {
    const std::string ini = "[Window][Mixer]\nViewportPos=100,200\nPos=0,0\nSize=800,600\n";
    CHECK(rescueFloatingWindows(ini, {{0, 0, 1920, 1080}}) == ini);
}
