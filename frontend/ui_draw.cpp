// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui_draw.h"
#include <algorithm>
namespace bvb::ui {
void rectangle(std::vector<std::uint8_t>& pixels, int x, int y, int w, int h, Color color, unsigned width, unsigned height) {
    for (int py = std::max(0, y); py < std::min(int(height), y + h); ++py)
        for (int px = std::max(0, x); px < std::min(int(width), x + w); ++px) {
            const auto i = (py * width + px) * 4;
            for (int c = 0; c < 3; ++c) pixels[i + c] = color[c];
            pixels[i + 3] = 255;
        }
}
// Five columns per character, seven rows, bit zero at the top.
const unsigned char* glyph(char c) {
    static const unsigned char letters[26][5] = {
        {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
        {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
        {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
        {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
        {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
        {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
        {3,4,120,4,3},{97,81,73,69,67}};
    static const unsigned char digits[10][5] = {
        {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},
        {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
        {54,73,73,73,54},{6,73,73,41,30}};
    static const unsigned char dot[5]={0,96,96,0,0}, slash[5]={32,16,8,4,2},
        percent[5]={99,19,8,100,99}, plus[5]={8,8,62,8,8}, dash[5]={8,8,8,8,8},
        colon[5]={0,36,36,0,0}, backslash[5]={2,4,8,16,32}, question[5]={2,1,81,9,6}, blank[5]={};
    if (c >= 'A' && c <= 'Z') return letters[c-'A'];
    if (c >= '0' && c <= '9') return digits[c-'0'];
    switch(c) { case '.': return dot; case '/': return slash; case '%': return percent;
        case '+': return plus; case '-': return dash; case ':': return colon;
        case '\\': return backslash; case '?': return question; default: return blank; }
}
void text(std::vector<std::uint8_t>& pixels, int x, int y, const std::string& value, Color color, int scale, unsigned width, unsigned height) {
    for (char c : value) {
        const auto* shape = glyph(c);
        for (int column = 0; column < 5; ++column) for (int row = 0; row < 7; ++row)
            if (shape[column] & (1 << row)) rectangle(pixels, x + column * scale, y + row * scale, scale, scale, color, width, height);
        x += 6 * scale;
    }
}
}
