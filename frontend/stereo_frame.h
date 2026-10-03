// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bvb {
constexpr unsigned eye_width = 384;
constexpr unsigned eye_height = 224;
struct StereoFrame {
    // Opaque RGBA8, top to bottom, ready for a graphics upload.
    std::vector<std::uint8_t> left;
    std::vector<std::uint8_t> right;
    // Gameplay defaults to native size; presentation screens may use a larger canvas.
    unsigned width = eye_width, height = eye_height;
};
bool valid_stereo_frame(const StereoFrame& frame);
// Input is libretro XRGB8888, side-by-side with separation set to zero.
// A null frame repeats the previous frame; pitch is bytes, not visible width.
bool copy_stereo_frame(const void* data, unsigned width, unsigned height,
                       std::size_t pitch, StereoFrame& output, bool swap_eyes = false);
}
