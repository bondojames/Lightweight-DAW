// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <shellapi.h>

#include "platform/win32/WindowHost.h"

#include "platform/win32/ShellIntegration.h"

#include <cmath>

namespace ldaw::platform {
namespace {

constexpr wchar_t kWindowClass[] = L"LightweightDAW.MainWindow";
constexpr UINT_PTR kModalTimerId = 1;
constexpr UINT kModalTimerMs = 16;

BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM userData) {
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        const RECT& r = info.rcWork;
        reinterpret_cast<std::vector<IntRect>*>(userData)->push_back({r.left, r.top, r.right - r.left, r.bottom - r.top});
    }
    return TRUE;
}

} // namespace

WindowHost::~WindowHost() {
    if (frameTimer_) CloseHandle(frameTimer_);
    if (hwnd_) DestroyWindow(hwnd_);
    UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
}

bool WindowHost::create(const std::wstring& title, const IntRect& bounds, bool maximized) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = reinterpret_cast<WNDPROC>(&WindowHost::windowProc);
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) return false;

    maximizedOnCreate_ = maximized;
    hwnd_ = CreateWindowExW(0, kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW, bounds.x, bounds.y, bounds.w, bounds.h,
                            nullptr, nullptr, wc.hInstance, this);
    if (!hwnd_) return false;
    DragAcceptFiles(hwnd_, TRUE);
    return true;
}

void WindowHost::show(int nativeShowCommand) {
    ShowWindow(hwnd_, maximizedOnCreate_ && nativeShowCommand == SW_SHOWDEFAULT ? SW_SHOWMAXIMIZED : nativeShowCommand);
    UpdateWindow(hwnd_);
}

bool WindowHost::pumpMessages() {
    MSG msg;
    bool keepRunning = true;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) keepRunning = false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return keepRunning;
}

bool WindowHost::waitForMessages(double timeoutSeconds) {
    if (timeoutSeconds < 0.0) {
        return MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_OBJECT_0;
    }
    if (!frameTimer_) {
        frameTimer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    }
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(timeoutSeconds * 1.0e7); // Negative means relative, in 100 ns units.
    if (frameTimer_ && timeoutSeconds > 0.0 && SetWaitableTimer(frameTimer_, &due, 0, nullptr, nullptr, FALSE)) {
        // Plain timeouts round up to the 15.6 ms system tick, which held 60 Hz down
        // to about 49 Hz. This timer is accurate without changing the global tick.
        const HANDLE handles[] = {frameTimer_};
        const DWORD woke = MsgWaitForMultipleObjectsEx(1, handles, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        return woke == WAIT_OBJECT_0 + 1;
    }
    const DWORD timeout = static_cast<DWORD>(std::ceil(timeoutSeconds * 1000.0));
    return MsgWaitForMultipleObjectsEx(0, nullptr, timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_OBJECT_0;
}

bool WindowHost::takePendingResize(unsigned& width, unsigned& height) {
    if (pendingWidth_ == 0 || pendingHeight_ == 0) return false;
    width = pendingWidth_;
    height = pendingHeight_;
    pendingWidth_ = pendingHeight_ = 0;
    return true;
}

bool WindowHost::takeDisplayChanged() {
    const bool changed = displayChanged_;
    displayChanged_ = false;
    return changed;
}

std::vector<std::filesystem::path> WindowHost::takeDroppedFiles() {
    auto files = std::move(dropped_);
    dropped_.clear();
    return files;
}

bool WindowHost::isMinimized() const { return IsIconic(hwnd_) != FALSE; }
bool WindowHost::isMaximized() const { return IsZoomed(hwnd_) != FALSE; }

IntRect WindowHost::restoredBounds() const {
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    GetWindowPlacement(hwnd_, &placement);
    const RECT& r = placement.rcNormalPosition;
    return {r.left, r.top, r.right - r.left, r.bottom - r.top};
}

float WindowHost::dpiScale() const { return static_cast<float>(GetDpiForWindow(hwnd_)) / 96.0f; }

std::vector<IntRect> WindowHost::monitorWorkAreas() {
    std::vector<IntRect> monitors;
    EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&monitors));
    return monitors;
}

long long __stdcall WindowHost::windowProc(HWND hwnd, unsigned message, unsigned long long wParam, long long lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<WindowHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, message, wParam, lParam);
    if (self->hook_ && self->hook_(hwnd, message, wParam, lParam)) return 1;
    if (message == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        self->hwnd_ = nullptr;
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    self->hwnd_ = hwnd;
    return self->handleMessage(message, wParam, lParam);
}

long long WindowHost::handleMessage(unsigned message, unsigned long long wParam, long long lParam) {
    switch (message) {
    case WM_SIZE:
        // Queued: the swap chain is resized between frames, on the UI thread.
        if (wParam != SIZE_MINIMIZED) {
            pendingWidth_ = LOWORD(lParam);
            pendingHeight_ = HIWORD(lParam);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        const float scale = hwnd_ ? dpiScale() : 1.0f;
        info->ptMinTrackSize.x = static_cast<LONG>(640 * scale);
        info->ptMinTrackSize.y = static_cast<LONG>(400 * scale);
        return 0;
    }
    case WM_ENTERSIZEMOVE:
        // Windows runs its own message loop while dragging the frame, so keep the
        // UI drawing from a timer until it ends.
        SetTimer(hwnd_, kModalTimerId, kModalTimerMs, nullptr);
        return 0;
    case WM_EXITSIZEMOVE:
        KillTimer(hwnd_, kModalTimerId);
        return 0;
    case WM_TIMER:
        if (wParam == kModalTimerId && modalTick_) modalTick_();
        return 0;
    case WM_DISPLAYCHANGE:
        displayChanged_ = true;
        break;
    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETWORKAREA) displayChanged_ = true;
        break;
    case WM_DROPFILES: {
        const HDROP drop = reinterpret_cast<HDROP>(wParam);
        auto files = droppedFiles(drop);
        DragFinish(drop);
        dropped_.insert(dropped_.end(), files.begin(), files.end());
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0; // Alt must not open the system menu.
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

} // namespace ldaw::platform
