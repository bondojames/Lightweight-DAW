// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <string>
#include <vector>


namespace ldaw::platform {

std::string toUtf8(const std::wstring& wide);
std::wstring toWide(const std::string& utf8);

std::filesystem::path executableDirectory();

// Settings and logs go in %LOCALAPPDATA%\LightweightDAW by default. A portable.flag
// file beside the EXE moves them there, but only if that folder is writable.
std::filesystem::path resolveDataDirectory();

std::vector<std::filesystem::path> droppedFiles(void* dropHandle);

} // namespace ldaw::platform
