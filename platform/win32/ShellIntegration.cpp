// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include "platform/win32/ShellIntegration.h"

#include <fstream>
#include <system_error>

namespace ldaw::platform {

std::string toUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring toWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), size);
    return out;
}

std::filesystem::path executableDirectory() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path resolveDataDirectory() {
    const auto exeDir = executableDirectory();
    std::error_code ec;
    if (std::filesystem::exists(exeDir / L"portable.flag", ec)) {
        const auto probe = exeDir / L".write_probe";
        const bool writable = static_cast<bool>(std::ofstream(probe));
        std::filesystem::remove(probe, ec);
        if (writable) return exeDir / L"data";
    }
    PWSTR localAppData = nullptr;
    std::filesystem::path result = exeDir / L"data";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppData))) {
        result = std::filesystem::path(localAppData) / L"LightweightDAW";
    }
    CoTaskMemFree(localAppData);
    return result;
}

std::vector<std::filesystem::path> droppedFiles(void* dropHandle) {
    const HDROP drop = static_cast<HDROP>(dropHandle);
    std::vector<std::filesystem::path> files;
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    files.reserve(count);
    for (UINT i = 0; i < count; ++i) {
        const UINT length = DragQueryFileW(drop, i, nullptr, 0);
        std::wstring name(length + 1, L'\0');
        DragQueryFileW(drop, i, name.data(), length + 1);
        name.resize(length);
        files.emplace_back(name);
    }
    return files;
}

} // namespace ldaw::platform
