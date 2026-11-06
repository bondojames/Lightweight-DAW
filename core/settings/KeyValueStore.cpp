// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/settings/KeyValueStore.h"

#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>

namespace ldaw {

void KeyValueStore::parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        values_[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

std::string KeyValueStore::serialize() const {
    std::string out;
    for (const auto& [key, value] : values_) out += key + "=" + value + "\n";
    return out;
}

bool KeyValueStore::loadFile(const std::filesystem::path& path) {
    bool ok = false;
    const std::string text = readTextFile(path, &ok);
    if (ok) parse(text);
    return ok;
}

bool KeyValueStore::saveFile(const std::filesystem::path& path) const {
    return atomicWriteText(path, serialize());
}

std::string KeyValueStore::getString(const std::string& key, const std::string& fallback) const {
    const auto it = values_.find(key);
    return it == values_.end() ? fallback : it->second;
}

int KeyValueStore::getInt(const std::string& key, int fallback) const {
    const auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    int value = 0;
    const char* first = it->second.data();
    const char* last = first + it->second.size();
    const auto result = std::from_chars(first, last, value);
    return (result.ec == std::errc{} && result.ptr == last) ? value : fallback;
}

void KeyValueStore::setString(const std::string& key, const std::string& value) { values_[key] = value; }
void KeyValueStore::setInt(const std::string& key, int value) { values_[key] = std::to_string(value); }

bool atomicWriteText(const std::filesystem::path& path, const std::string& text) {
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    auto temp = path;
    temp += L".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) return false;
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}

std::string readTextFile(const std::filesystem::path& path, bool* ok) {
    std::ifstream in(path, std::ios::binary);
    if (ok) *ok = static_cast<bool>(in);
    if (!in) return {};
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

} // namespace ldaw
