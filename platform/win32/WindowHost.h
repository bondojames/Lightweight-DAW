// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/layout/Geometry.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#ifndef _WINDEF_
struct HWND__;
typedef HWND__* HWND;
#endif

namespace ldaw::platform {

// Owns the primary window and its message handling. It knows nothing about ImGui
// or Direct3D: those attach through the hook and callbacks below.
class WindowHost {
public:
    // Return true if the message was consumed.
    using MessageHook = std::function<bool(HWND, unsigned, unsigned long long, long long)>;

    WindowHost() = default;
    ~WindowHost();
    WindowHost(const WindowHost&) = delete;
    WindowHost& operator=(const WindowHost&) = delete;

    bool create(const std::wstring& title, const IntRect& bounds, bool maximized);
    void show(int nativeShowCommand);
    HWND hwnd() const { return hwnd_; }

    void setMessageHook(MessageHook hook) { hook_ = std::move(hook); }
    // Called during the modal resize/move loop, when the main loop cannot run.
    void setModalLoopTick(std::function<void()> tick) { modalTick_ = std::move(tick); }

    // Processes all queued messages. Returns false after WM_QUIT.
    bool pumpMessages();
    // Sleeps until a message arrives or the timeout passes; true means a message
    // woke us. A negative timeout waits only for messages.
    bool waitForMessages(double timeoutSeconds);

    bool takePendingResize(unsigned& width, unsigned& height);
    bool takeDisplayChanged();
    std::vector<std::filesystem::path> takeDroppedFiles();
    // Where the last drop landed, in screen pixels.
    int lastDropX() const { return dropX_; }
    int lastDropY() const { return dropY_; }

    bool isMinimized() const;
    bool isMaximized() const;
    IntRect restoredBounds() const;
    float dpiScale() const;

    static std::vector<IntRect> monitorWorkAreas();

private:
    static long long __stdcall windowProc(HWND hwnd, unsigned message, unsigned long long wParam, long long lParam);
    long long handleMessage(unsigned message, unsigned long long wParam, long long lParam);

    HWND hwnd_ = nullptr;
    void* frameTimer_ = nullptr;
    bool maximizedOnCreate_ = false;
    MessageHook hook_;
    std::function<void()> modalTick_;
    unsigned pendingWidth_ = 0;
    unsigned pendingHeight_ = 0;
    bool displayChanged_ = false;
    int dropX_ = 0;
    int dropY_ = 0;
    std::vector<std::filesystem::path> dropped_;
};

} // namespace ldaw::platform
