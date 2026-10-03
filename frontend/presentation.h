// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "stereo_frame.h"
#include <array>

namespace bvb {
struct ScreenSettings {
    float width = 1.2f;
    float distance = 2.0f;
    bool swap_eyes = false;
    void clamp();
    float height() const { return width * float(eye_height) / float(eye_width); }
};
struct ScreenAnchor {
    float x = 0, y = 0, z = 0;
    float yaw = 0;
};
struct ScreenPose {
    std::array<float, 3> position;
    // OpenXR quaternion component order: x, y, z, w.
    std::array<float, 4> orientation;
};
ScreenPose screen_pose(const ScreenAnchor& anchor, const ScreenSettings& settings);
StereoFrame make_test_pattern();
// RGBA input -> BGRA for DXGI B8G8R8A8 formats or a Windows DIB preview.
std::vector<std::uint8_t> rgba_to_bgra(const std::vector<std::uint8_t>& pixels);
// Pack variable-resolution eyes into top-down BGRA rows for the desktop preview.
std::vector<std::uint8_t> stereo_preview_pixels(const StereoFrame& frame, bool swap_eyes);
}
