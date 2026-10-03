// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace bvb {
// Runs game frames at the core's rate, independent of the display refresh rate.
class FrameClock {
    double rate;
    double accumulated = 0;
public:
    explicit FrameClock(double fps) : rate(fps) {
        if (!std::isfinite(fps) || fps < 1 || fps > 1000)
            throw std::invalid_argument("Invalid core frame rate");
    }
    unsigned advance(double elapsed_seconds, bool active = true) {
        if (!active) { accumulated = 0; return 0; }
        if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0) elapsed_seconds = 0;
        // Bound catch-up work after a debugger stop, window drag or runtime stall.
        accumulated += std::min(elapsed_seconds, 0.1) * rate;
        unsigned frames = static_cast<unsigned>(std::floor(accumulated + 1e-9));
        accumulated -= frames;
        return std::min(frames, 5u);
    }
};
}
