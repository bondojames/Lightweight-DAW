// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include "platform/win32/NativeDialogs.h"

using Microsoft::WRL::ComPtr;

namespace ldaw::platform {
namespace {

std::optional<std::filesystem::path> runDialog(IFileDialog* dialog, HWND owner) {
    if (FAILED(dialog->Show(owner))) return std::nullopt; // Includes user cancel.
    ComPtr<IShellItem> item;
    if (FAILED(dialog->GetResult(&item))) return std::nullopt;
    PWSTR name = nullptr;
    if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &name))) return std::nullopt;
    std::filesystem::path path(name);
    CoTaskMemFree(name);
    return path;
}

} // namespace

std::optional<std::filesystem::path> showOpenFileDialog(HWND owner) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return std::nullopt;
    return runDialog(dialog.Get(), owner);
}

std::optional<std::filesystem::path> showSaveFileDialog(HWND owner) {
    ComPtr<IFileSaveDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return std::nullopt;
    return runDialog(dialog.Get(), owner);
}

} // namespace ldaw::platform
