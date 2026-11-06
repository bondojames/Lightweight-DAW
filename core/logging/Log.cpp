// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/logging/Log.h"

#include <chrono>
#include <format>
#include <fstream>
#include <mutex>
#include <system_error>

namespace ldaw::log {
namespace {

std::mutex g_mutex;
std::ofstream g_file;

void write(const char* level, const std::string& message) {
    const std::scoped_lock lock(g_mutex);
    if (!g_file.is_open()) return;
    const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    g_file << std::format("{:%FT%TZ} {} {}\n", now, level, message);
    g_file.flush();
}

} // namespace

void open(const std::filesystem::path& file) {
    const std::scoped_lock lock(g_mutex);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    g_file.open(file, std::ios::binary | std::ios::trunc);
}

void info(const std::string& message) { write("INFO ", message); }
void error(const std::string& message) { write("ERROR", message); }

void close() {
    const std::scoped_lock lock(g_mutex);
    g_file.close();
}

} // namespace ldaw::log
