// SPDX-License-Identifier: GPL-2.0-or-later
#include "stereo_frame.h"
#include <cstring>

namespace bvb {
bool valid_stereo_frame(const StereoFrame& frame) {
    return frame.width && frame.height && frame.width <= 4096 && frame.height <= 4096
        && frame.left.size() == std::size_t(frame.width) * frame.height * 4
        && frame.right.size() == frame.left.size();
}
bool copy_stereo_frame(const void* data, unsigned width, unsigned height,
                       std::size_t pitch, StereoFrame& output, bool swap_eyes) {
    if (!data) return false;
    if (width != eye_width * 2 || height != eye_height ||
        pitch < width * sizeof(std::uint32_t)) return false;
    output.left.resize(eye_width * eye_height * 4);
    output.right.resize(eye_width * eye_height * 4);
    output.width = eye_width; output.height = eye_height;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (unsigned eye = 0; eye < 2; ++eye) {
        auto& dest = (eye ^ unsigned(swap_eyes)) ? output.right : output.left;
        for (unsigned y = 0; y < eye_height; ++y) {
            for (unsigned x = 0; x < eye_width; ++x) {
                std::uint32_t pixel;
                std::memcpy(&pixel, bytes + y * pitch + (x + eye * eye_width) * 4, 4);
                const auto offset = (y * eye_width + x) * 4;
                dest[offset] = (pixel >> 16) & 255;
                dest[offset + 1] = (pixel >> 8) & 255;
                dest[offset + 2] = pixel & 255;
                dest[offset + 3] = 255;
            }
        }
    }
    return true;
}
}
