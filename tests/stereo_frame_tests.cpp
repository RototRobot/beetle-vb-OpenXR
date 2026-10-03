// SPDX-License-Identifier: GPL-2.0-or-later
#include "stereo_frame.h"
#include <iostream>
#include <stdexcept>
#include <cstring>

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
int main() {
    try {
        // Upstream reserves 1024 pixels per row for a visible 768-pixel image.
        constexpr std::size_t pitch = 1024 * 4;
        std::vector<std::uint8_t> input(pitch * bvb::eye_height, 0xCC);
        for (unsigned y = 0; y < bvb::eye_height; ++y) {
            for (unsigned x = 0; x < 768; ++x) {
                std::uint32_t pixel = (x < 384 ? 0x00112233 : 0x00445566) + y;
                std::memcpy(input.data() + y * pitch + x * 4, &pixel, 4);
            }
        }
        bvb::StereoFrame frame;
        require(bvb::copy_stereo_frame(input.data(), 768, 224, pitch, frame), "valid frame rejected");
        require(frame.left.size() == 384 * 224 * 4, "wrong eye dimensions");
        require(frame.left[0] == 0x11 && frame.left[1] == 0x22 && frame.left[2] == 0x33 && frame.left[3] == 255, "left channels");
        require(frame.right[0] == 0x44 && frame.right[2] == 0x66, "right channels");
        require(frame.left[384 * 4 + 2] == 0x34, "pitch ignored");
        require(frame.right[(384 * 223 + 383) * 4 + 2] == ((0x66 + 223) & 255), "last pixel");
        const auto original = frame.left;
        require(!bvb::copy_stereo_frame(nullptr, 768, 224, pitch, frame), "duplicate handling");
        require(frame.left == original, "duplicate modified previous frame");
        require(!bvb::copy_stereo_frame(input.data(), 384, 224, pitch, frame), "mono accepted");
        require(!bvb::copy_stereo_frame(input.data(), 768, 223, pitch, frame), "bad height accepted");
        require(!bvb::copy_stereo_frame(input.data(), 768, 224, 767 * 4, frame), "short pitch accepted");
        require(frame.left == original, "invalid frame modified output");
        require(bvb::copy_stereo_frame(input.data(), 768, 224, pitch, frame, true), "swap rejected");
        require(frame.right == original && frame.left[0] == 0x44, "eye swap failed");
        require(bvb::valid_stereo_frame(frame),"Native frame metadata invalid");
        frame.width=1536; frame.height=896;
        require(!bvb::valid_stereo_frame(frame),"Dimensions mismatched to buffers accepted");
        frame.left.resize(std::size_t(frame.width)*frame.height*4); frame.right=frame.left;
        require(bvb::valid_stereo_frame(frame),"Complete high resolution frame rejected");
        frame.height=0; require(!bvb::valid_stereo_frame(frame),"Zero dimensions accepted");
        require(bvb::copy_stereo_frame(input.data(),768,224,pitch,frame),"Native conversion after UI frame failed");
        require(frame.width==384 && frame.height==224 && bvb::valid_stereo_frame(frame),"Native conversion retained UI dimensions");
        std::cout << "Stereo dimensions, padded pitch, channels, eye order, duplicate and invalid frames passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
