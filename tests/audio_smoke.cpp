// SPDX-License-Identifier: GPL-2.0-or-later
#include "audio_output.h"
#include <chrono>
#include <thread>
#include <iostream>
// Manual device check, deliberately not in CTest: CI may have no audio endpoint.
int main() {
    try {
        bvb::AudioOutput output;
        output.initialize(44100, 0); // Silent; tests submission/lifetime rather than audible quality.
        std::vector<std::int16_t> pcm(882 * 2, 0);
        for (unsigned cycle = 0; cycle < 2; ++cycle) {
            output.set_active(true);
            for (unsigned i = 0; i < 20; ++i) {
                output.submit(pcm);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            output.set_muted(true);
            output.set_active(false); // Destroy/recreate while buffers may be in flight.
            output.set_muted(false);
        }
        std::cout << "Default audio device, silent PCM submission, mute and pause/resume completed. Audible game sound is untested.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
