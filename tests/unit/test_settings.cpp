// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/settings/KeyValueStore.h"
#include "tests/unit/test_framework.h"

#include <filesystem>

using namespace ldaw;

TEST_CASE(settings_round_trip) {
    KeyValueStore a;
    a.setInt("window.w", 1280);
    a.setString("theme", "dark");
    KeyValueStore b;
    b.parse(a.serialize());
    CHECK(b.getInt("window.w", 0) == 1280);
    CHECK(b.getString("theme", "") == "dark");
}

TEST_CASE(settings_tolerates_garbage) {
    KeyValueStore s;
    s.parse("no equals sign\n=novalue\n# comment\r\nn=12abc\nok=7\r\n");
    CHECK(s.getInt("n", -1) == -1);
    CHECK(s.getInt("ok", -1) == 7);
    CHECK(s.getInt("missing", 5) == 5);
}

TEST_CASE(settings_keeps_unknown_keys) {
    KeyValueStore s;
    s.parse("future.key=abc\n");
    CHECK(s.serialize().find("future.key=abc") != std::string::npos);
}

TEST_CASE(atomic_write_replaces_and_handles_non_ascii_paths) {
    const auto dir = std::filesystem::temp_directory_path() / L"ldaw_test_\u00e9\u97f3";
    const auto file = dir / L"s.ini";
    CHECK(atomicWriteText(file, "one"));
    CHECK(atomicWriteText(file, "two"));
    bool ok = false;
    CHECK(readTextFile(file, &ok) == "two");
    CHECK(ok);
    auto temp = file;
    temp += L".tmp";
    CHECK(!std::filesystem::exists(temp));
    std::filesystem::remove_all(dir);
}

TEST_CASE(read_missing_file_reports_failure) {
    bool ok = true;
    CHECK(readTextFile(L"Z:\\does\\not\\exist.ini", &ok).empty());
    CHECK(!ok);
}
