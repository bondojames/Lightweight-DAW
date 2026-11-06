// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace ldaw::ui {

// 0 = dark, 1 = light, 2 = high contrast. Sizes are rebuilt from the base style so
// repeated DPI changes do not compound.
void applyTheme(int themeId, float dpiScale);

} // namespace ldaw::ui
