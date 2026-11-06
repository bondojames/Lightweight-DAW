// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include "app/Application.h"
#include "app/CommandLine.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const ldaw::CommandLine args = ldaw::parseCommandLine(argc, argv);
    LocalFree(argv);

    int exitCode = 1;
    {
        ldaw::Application app(args);
        exitCode = app.run(showCommand);
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return exitCode;
}
