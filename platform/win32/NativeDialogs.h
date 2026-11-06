// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <optional>

struct HWND__;
typedef HWND__* HWND;

namespace ldaw::platform {

// Windows Common Item Dialogs. These run a nested modal message loop, so call them
// between UI frames, never from inside one. Audio is unaffected; the UI just stops
// redrawing until the dialog closes. nullopt means cancelled.
std::optional<std::filesystem::path> showOpenFileDialog(HWND owner);
// Lists the audio formats the app can import, plus an "all files" choice.
std::optional<std::filesystem::path> showOpenAudioDialog(HWND owner);
std::optional<std::filesystem::path> showSaveFileDialog(HWND owner);

} // namespace ldaw::platform
