// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "vr_input.h"
#include <openxr/openxr.h>

namespace bvb {
class OpenXrInput {
public:
    ~OpenXrInput();
    OpenXrInput() = default;
    OpenXrInput(const OpenXrInput&) = delete;
    OpenXrInput& operator=(const OpenXrInput&) = delete;
    void initialize(XrInstance instance, XrSession session);
    VrInput poll(bool focused, float deadzone = vr_default_deadzone);
    void profiles_changed();
private:
    XrInstance instance = XR_NULL_HANDLE;
    XrSession session = XR_NULL_HANDLE;
    XrActionSet action_set = XR_NULL_HANDLE;
    std::array<XrAction, static_cast<std::size_t>(VrAction::count)> actions{};
    std::array<XrPath, 2> hands{};
    std::array<bool, 2> last_active{};
    VrInputMapper mapper;
    XrPath path(const char* text);
    XrActionStateBoolean boolean(VrAction action, XrPath hand);
};
}
