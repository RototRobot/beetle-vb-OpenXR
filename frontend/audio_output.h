// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
namespace bvb {
class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    void initialize(unsigned sample_rate, float volume);
    void set_active(bool active); // Pause discards queued audio; resume primes again.
    void set_muted(bool muted);
    void set_volume(float volume);
    void submit(const std::vector<std::int16_t>& interleaved_stereo);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
