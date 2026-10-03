// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "presentation.h"
#include <memory>
#include <string>
namespace bvb {
class DesktopWindow {
public:
    DesktopWindow(const StereoFrame& frame, ScreenSettings& screen, bool player = false,
                  const std::wstring& title = L"Beetle VB OpenXR - Stereo screen test");
    ~DesktopWindow();
    DesktopWindow(const DesktopWindow&) = delete;
    DesktopWindow& operator=(const DesktopWindow&) = delete;
    void pump();
    void refresh(std::uint64_t frame_number = 0, bool effective_paused = false);
    bool foreground() const;
    bool close_requested = false, recenter_requested = false, paused = false, muted = false;
    bool menu_requested = false;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
