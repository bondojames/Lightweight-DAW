// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <dbghelp.h>

#include "platform/win32/CrashHandler.h"

#include <cstdio>
#include <system_error>

namespace ldaw::platform {
namespace {

// Pre-built so the handler does not allocate while the process is failing.
wchar_t g_dumpPrefix[1024] = {};

LONG WINAPI onUnhandledException(EXCEPTION_POINTERS* info) {
    wchar_t path[1100];
    swprintf_s(path, L"%ls\\crash-%lu.dmp", g_dumpPrefix, GetCurrentProcessId());
    const HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION exception{GetCurrentThreadId(), info, FALSE};
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &exception, nullptr, nullptr);
        CloseHandle(file);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void installCrashHandler(const std::filesystem::path& dumpDirectory) {
    std::error_code ec;
    std::filesystem::create_directories(dumpDirectory, ec);
    wcsncpy_s(g_dumpPrefix, dumpDirectory.c_str(), _TRUNCATE);
    SetUnhandledExceptionFilter(onUnhandledException);
}

} // namespace ldaw::platform
