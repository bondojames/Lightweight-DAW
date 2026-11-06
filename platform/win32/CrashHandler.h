// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>

namespace ldaw::platform {

// Writes a minidump to the given folder on an unhandled exception. Dumps stay on
// this machine; nothing is ever uploaded.
void installCrashHandler(const std::filesystem::path& dumpDirectory);

} // namespace ldaw::platform
