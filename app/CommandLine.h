// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace ldaw {

enum class ProfileMode { None, Idle, Active, Minimized, Detached, Playing };

struct CommandLine {
    bool warp = false;                          // Diagnostic software rendering; off by default.
    bool testCrash = false;                     // Deliberately crashes, to prove dumps are written.
    bool uiSelfTest = false;                    // Scripted UI checks; exit code 0 means all passed.
    double smokeSeconds = 0.0;                  // Run this long, then exit with a status code.
    std::optional<std::filesystem::path> dataDir;
    ProfileMode profile = ProfileMode::None;
    double settleSeconds = 60.0;
    double profileSeconds = 60.0;
    std::filesystem::path profileOut;
};

CommandLine parseCommandLine(int argc, wchar_t** argv);
const char* profileModeName(ProfileMode mode);

} // namespace ldaw
