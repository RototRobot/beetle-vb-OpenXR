// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "stereo_frame.h"
#include <array>
#include <string>
namespace bvb::ui {
using Color = std::array<std::uint8_t, 3>;
void rectangle(std::vector<std::uint8_t>& pixels, int x, int y, int w, int h, Color color,
               unsigned width = eye_width, unsigned height = eye_height);
void text(std::vector<std::uint8_t>& pixels, int x, int y, const std::string& value, Color color, int scale = 1,
          unsigned width = eye_width, unsigned height = eye_height);
}
