// SPDX-License-Identifier: GPL-2.0-or-later
#include "presentation.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool near(float a, float b) { return std::abs(a - b) < 0.0001f; }
bool rgb(const std::vector<std::uint8_t>& pixels, unsigned x, unsigned y, unsigned r, unsigned g, unsigned b) {
    auto i = (y * bvb::eye_width + x) * 4;
    return pixels[i] == r && pixels[i+1] == g && pixels[i+2] == b && pixels[i+3] == 255;
}
}
int main() {
    try {
        bvb::ScreenSettings settings;
        require(near(settings.height() / settings.width, 224.f / 384.f), "Screen aspect ratio changed");
        auto pose = bvb::screen_pose({1, 1.5f, 3, 0}, settings);
        require(near(pose.position[0], 1) && near(pose.position[1], 1.5f) && near(pose.position[2], 1), "Screen is not in front of head");
        require(near(pose.orientation[3], 1), "Default screen rotation");
        constexpr float pi = 3.14159265358979323846f;
        pose = bvb::screen_pose({1, 1.5f, 3, pi / 2}, settings);
        require(near(pose.position[0], -1) && near(pose.position[2], 3), "Recenter yaw ignores head direction");
        float norm = 0;
        for (auto component : pose.orientation) norm += component * component;
        require(near(norm, 1), "Screen orientation is not a unit quaternion");
        settings.width = -100;
        settings.distance = 100;
        settings.clamp();
        require(near(settings.width, 0.3f) && near(settings.distance, 5), "Setting limits not enforced");
        settings.width = std::numeric_limits<float>::quiet_NaN();
        settings.distance = std::numeric_limits<float>::infinity();
        settings.clamp();
        require(near(settings.width, 1.2f) && near(settings.distance, 2), "Non-finite settings not recovered");

        const auto frame = bvb::make_test_pattern();
        require(frame.left.size() == 384 * 224 * 4 && frame.right.size() == frame.left.size(), "Pattern dimensions");
        for (std::size_t i = 3; i < frame.left.size(); i += 4)
            require(frame.left[i] == 255 && frame.right[i] == 255, "Pattern has transparent pixels");
        for (const auto* eye : {&frame.left, &frame.right}) {
            require(rgb(*eye, 14, 200, 255,0,0), "Red bar channel error");
            require(rgb(*eye, 136, 200, 0,255,0), "Green bar channel error");
            require(rgb(*eye, 258, 200, 0,0,255), "Blue bar channel error");
            require(rgb(*eye, 0, 0, 255,255,255) && rgb(*eye, 383, 223, 255,255,255), "Pattern border clipped");
            require(rgb(*eye, 192, 104, 255,255,255), "Common screen-plane marker missing");
        }
        require(frame.left != frame.right, "Eyes received identical images");
        // Match the green target's intended crossed disparity, not just eye labels.
        require(rgb(frame.left, 200, 70, 0,255,0) && rgb(frame.right, 180, 70, 0,255,0), "Near target eye disparity");
        require(!rgb(frame.left, 180, 70, 0,255,0) && !rgb(frame.right, 200, 70, 0,255,0), "Near target accidentally duplicated");
        require(rgb(frame.left, 180, 140, 255,0,255) && rgb(frame.right, 200, 140, 255,0,255), "Far target eye disparity");
        const auto converted = bvb::rgba_to_bgra({0x12,0x34,0x56,0x78, 1,2,3,255});
        require(converted == std::vector<std::uint8_t>{0x56,0x34,0x12,0x78, 3,2,1,255}, "BGRA conversion corrupts channels/alpha");
        bool rejected = false;
        try { bvb::rgba_to_bgra({1,2,3}); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Malformed pixels accepted");
        bvb::StereoFrame tiny; tiny.width=2; tiny.height=2;
        tiny.left={1,2,3,255, 4,5,6,255, 7,8,9,255, 10,11,12,255};
        tiny.right={21,22,23,255, 24,25,26,255, 27,28,29,255, 30,31,32,255};
        const auto preview=bvb::stereo_preview_pixels(tiny,false);
        require(preview.size()==32 && preview[0]==3 && preview[8]==23 && preview[16]==9 && preview[24]==29,
            "Variable-size preview mixed rows, eyes or channels");
        const auto swapped=bvb::stereo_preview_pixels(tiny,true);
        require(swapped[0]==23 && swapped[8]==3 && swapped[16]==29 && swapped[24]==9,"Variable-size preview swap failed");
        tiny.width=0; rejected=false;
        try {bvb::stereo_preview_pixels(tiny,false);} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected,"Preview accepted invalid dimensions");
        std::cout << "Screen geometry, limits, stereo disparity, color bars, alpha and BGRA conversion passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
