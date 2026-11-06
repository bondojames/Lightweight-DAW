// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/CommandLine.h"

#include <cstdlib>
#include <string_view>

namespace ldaw {

CommandLine parseCommandLine(int argc, wchar_t** argv) {
    CommandLine out;
    for (int i = 1; i < argc; ++i) {
        const std::wstring_view arg = argv[i];
        const wchar_t* value = (i + 1 < argc) ? argv[i + 1] : nullptr;
        if (arg == L"--warp") out.warp = true;
        else if (arg == L"--test-crash") out.testCrash = true;
        else if (arg == L"--ui-self-test") out.uiSelfTest = true;
        else if (arg == L"--smoke-seconds" && value) { out.smokeSeconds = std::wcstod(value, nullptr); ++i; }
        else if (arg == L"--data-dir" && value) { out.dataDir = value; ++i; }
        else if (arg == L"--settle-seconds" && value) { out.settleSeconds = std::wcstod(value, nullptr); ++i; }
        else if (arg == L"--profile-seconds" && value) { out.profileSeconds = std::wcstod(value, nullptr); ++i; }
        else if (arg == L"--profile-out" && value) { out.profileOut = value; ++i; }
        else if (arg == L"--profile" && value) {
            const std::wstring_view mode = value;
            if (mode == L"idle") out.profile = ProfileMode::Idle;
            else if (mode == L"active") out.profile = ProfileMode::Active;
            else if (mode == L"minimized") out.profile = ProfileMode::Minimized;
            else if (mode == L"detached") out.profile = ProfileMode::Detached;
            ++i;
        }
    }
    return out;
}

const char* profileModeName(ProfileMode mode) {
    switch (mode) {
    case ProfileMode::None: return "none";
    case ProfileMode::Idle: return "idle";
    case ProfileMode::Active: return "active";
    case ProfileMode::Minimized: return "minimized";
    case ProfileMode::Detached: return "detached";
    }
    return "unknown";
}

} // namespace ldaw
