// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <map>
#include <string>

namespace ldaw {

// User preferences as "key=value" lines. Unknown keys survive a load/save, and
// malformed lines are skipped, so a damaged file can never stop the app starting.
class KeyValueStore {
public:
    void parse(const std::string& text);
    std::string serialize() const;

    bool loadFile(const std::filesystem::path& path);
    bool saveFile(const std::filesystem::path& path) const;

    std::string getString(const std::string& key, const std::string& fallback) const;
    int getInt(const std::string& key, int fallback) const;
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);

private:
    std::map<std::string, std::string> values_;
};

// Writes to a sibling temp file, then renames over the target, so a crash can
// leave a stale temp file but never a half-written target.
bool atomicWriteText(const std::filesystem::path& path, const std::string& text);
std::string readTextFile(const std::filesystem::path& path, bool* ok = nullptr);

} // namespace ldaw
