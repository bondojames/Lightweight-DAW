// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <string>

namespace ldaw::log {

// Blocking file logger for the UI and worker threads. It takes a lock and does disk
// I/O, so it must never be called from an audio callback (spec section 4.4).
void open(const std::filesystem::path& file);
void info(const std::string& message);
void error(const std::string& message);
void close();

} // namespace ldaw::log
