// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "presentation.h"
#include "vr_input.h"
#include <memory>

namespace bvb {
// Windows D3D11 backend. Runtime handles and borrowed textures stay private.
class OpenXrSession {
public:
    OpenXrSession();
    ~OpenXrSession();
    OpenXrSession(const OpenXrSession&) = delete;
    OpenXrSession& operator=(const OpenXrSession&) = delete;
    // Optional larger UI swapchains; native gameplay has its own untouched pair.
    void initialize(bool enable_controllers = false, unsigned ui_scale = 1);
    VrInput poll_input(float deadzone = vr_default_deadzone);
    void poll_events();
    bool running() const;
    bool focused() const;
    bool finished() const;
    bool lost() const;
    void request_exit();
    void recenter();
    // Wait/begin/end one XR frame. Suppresses layers when shouldRender is false.
    void render_frame(const StereoFrame& frame, const ScreenSettings& settings);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
