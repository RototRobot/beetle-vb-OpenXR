// SPDX-License-Identifier: GPL-2.0-or-later
#include "presentation.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace bvb {
void ScreenSettings::clamp() {
    if (!std::isfinite(width)) width = 1.2f;
    if (!std::isfinite(distance)) distance = 2.0f;
    width = std::clamp(width, 0.3f, 3.0f);
    distance = std::clamp(distance, 0.5f, 5.0f);
}
ScreenPose screen_pose(const ScreenAnchor& anchor, const ScreenSettings& settings) {
    return {{anchor.x - std::sin(anchor.yaw) * settings.distance, anchor.y,
             anchor.z - std::cos(anchor.yaw) * settings.distance},
            {0, std::sin(anchor.yaw * 0.5f), 0, std::cos(anchor.yaw * 0.5f)}};
}
std::vector<std::uint8_t> rgba_to_bgra(const std::vector<std::uint8_t>& pixels) {
    if (pixels.size() % 4) throw std::invalid_argument("RGBA pixel buffer has incomplete pixels");
    auto output = pixels;
    for (std::size_t i = 0; i < output.size(); i += 4) std::swap(output[i], output[i + 2]);
    return output;
}
std::vector<std::uint8_t> stereo_preview_pixels(const StereoFrame& frame, bool swap_eyes) {
    if(!valid_stereo_frame(frame)) throw std::invalid_argument("Preview requires two complete eye images");
    std::vector<std::uint8_t> output(frame.left.size()*2);
    for(unsigned y=0;y<frame.height;++y) for(unsigned eye=0;eye<2;++eye) {
        const auto& source=(eye ^ unsigned(swap_eyes))?frame.right:frame.left;
        for(unsigned x=0;x<frame.width;++x) {
            const auto src=(std::size_t(y)*frame.width+x)*4;
            const auto dst=(std::size_t(y)*frame.width*2+eye*frame.width+x)*4;
            output[dst]=source[src+2]; output[dst+1]=source[src+1];
            output[dst+2]=source[src]; output[dst+3]=source[src+3];
        }
    }
    return output;
}
namespace {
using Color = std::array<std::uint8_t, 3>;
void rect(std::vector<std::uint8_t>& pixels, int x, int y, int width, int height, Color color) {
    for (int row = std::max(y, 0); row < std::min(y + height, int(eye_height)); ++row)
        for (int col = std::max(x, 0); col < std::min(x + width, int(eye_width)); ++col) {
            auto offset = (row * eye_width + col) * 4;
            std::copy(color.begin(), color.end(), pixels.begin() + offset);
            pixels[offset + 3] = 255;
        }
}
// Small original bitmap glyphs needed by this diagnostic; no font dependency.
std::array<unsigned, 7> glyph(char c) {
    switch (c) {
    case 'L': return {16,16,16,16,16,16,31};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'T': return {31,4,4,4,4,4,4};
    case 'R': return {30,17,17,30,20,18,17};
    case 'I': return {31,4,4,4,4,4,31};
    case 'G': return {14,17,16,23,17,17,14};
    case 'H': return {17,17,17,31,17,17,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'B': return {30,17,17,30,17,17,30};
    case 'M': return {17,27,21,21,17,17,17};
    default: return {};
    }
}
void text(std::vector<std::uint8_t>& pixels, int x, int y, const std::string& label, Color color) {
    for (char c : label) {
        auto rows = glyph(c);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (rows[row] & (16 >> col)) rect(pixels, x + col * 2, y + row * 2, 2, 2, color);
        x += 12;
    }
}
}
StereoFrame make_test_pattern() {
    StereoFrame frame;
    frame.left.resize(eye_width * eye_height * 4);
    frame.right.resize(eye_width * eye_height * 4);
    for (unsigned eye = 0; eye < 2; ++eye) {
        auto& pixels = eye ? frame.right : frame.left;
        rect(pixels, 0, 0, eye_width, eye_height, {8, 8, 12});
        for (int x = 0; x < int(eye_width); x += 24) rect(pixels, x, 0, 1, eye_height, {40,40,40});
        for (int y = 0; y < int(eye_height); y += 24) rect(pixels, 0, y, eye_width, 1, {40,40,40});
        rect(pixels, 0, 0, eye_width, 3, {255,255,255});
        rect(pixels, 0, eye_height - 3, eye_width, 3, {255,255,255});
        rect(pixels, 0, 0, 3, eye_height, {255,255,255});
        rect(pixels, eye_width - 3, 0, 3, eye_height, {255,255,255});
        text(pixels, 12, 12, eye ? "RIGHT" : "LEFT", eye ? Color{0,255,255} : Color{255,64,64});
        text(pixels, 170, 12, "TOP", {255,255,255});
        text(pixels, 156, 172, "BOTTOM", {255,255,255});
        // A common centre marker on the screen plane.
        rect(pixels, 180, 104, 24, 2, {255,255,255});
        rect(pixels, 191, 93, 2, 24, {255,255,255});
        // Crossed disparity in the upper green target; opposite in lower magenta.
        int shift = eye ? -6 : 6;
        rect(pixels, 184 + shift, 64, 16, 16, {0,255,0});
        rect(pixels, 184 - shift, 132, 16, 16, {255,0,255});
        // Identical color bars in both eyes expose channel swaps.
        rect(pixels, 12, 198, 116, 12, {255,0,0});
        rect(pixels, 134, 198, 116, 12, {0,255,0});
        rect(pixels, 256, 198, 116, 12, {0,0,255});
    }
    return frame;
}
}
