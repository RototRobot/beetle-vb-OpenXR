// SPDX-License-Identifier: GPL-2.0-or-later
#include "audio_output.h"
#include <windows.h>
#include <xaudio2.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace bvb {
namespace {
void check(HRESULT result, const char* call) {
    if (FAILED(result)) throw std::runtime_error(std::string(call) + " failed (HRESULT " + std::to_string(result) + ")");
}
}
struct AudioOutput::Impl {
    struct Slot {
        std::atomic<bool> busy{false};
        std::vector<std::int16_t> samples;
    };
    // Bounded storage outlives the voice and its callbacks; no callback allocations.
    std::array<Slot, 6> slots;
    struct Callback : IXAudio2VoiceCallback {
        std::atomic<HRESULT> error{S_OK};
        void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
        void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
        void STDMETHODCALLTYPE OnStreamEnd() override {}
        void STDMETHODCALLTYPE OnBufferStart(void*) override {}
        void STDMETHODCALLTYPE OnBufferEnd(void* context) override {
            static_cast<Slot*>(context)->busy.store(false, std::memory_order_release);
        }
        void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
        void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT value) override { error.store(value); }
    } callback;
    Microsoft::WRL::ComPtr<IXAudio2> engine;
    IXAudio2MasteringVoice* master = nullptr;
    IXAudio2SourceVoice* source = nullptr;
    WAVEFORMATEX format{};
    float volume = 1;
    bool muted = false, active = false, started = false, com_initialized = false;
    ~Impl() {
        destroy_source();
        if (master) master->DestroyVoice();
        engine.Reset();
        if (com_initialized) CoUninitialize();
    }
    void destroy_source() {
        if (source) { source->DestroyVoice(); source = nullptr; }
        // DestroyVoice waits until processing/callback use has finished.
        for (auto& slot : slots) slot.busy.store(false);
        started = false;
    }
    void create_source() {
        check(engine->CreateSourceVoice(&source, &format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &callback), "Create audio source");
        check(source->SetVolume(muted ? 0 : volume), "Set audio volume");
        callback.error.store(S_OK);
    }
    void initialize(unsigned rate, float requested_volume) {
        if (engine) throw std::logic_error("Audio output already initialized");
        if (rate < 8000 || rate > 192000 || !std::isfinite(requested_volume) || requested_volume < 0 || requested_volume > 1)
            throw std::invalid_argument("Invalid audio format or volume");
        auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (result != RPC_E_CHANGED_MODE) check(result, "Initialize COM for audio");
        com_initialized = SUCCEEDED(result);
        check(XAudio2Create(&engine, 0, XAUDIO2_DEFAULT_PROCESSOR), "Create XAudio2 engine");
        check(engine->CreateMasteringVoice(&master), "Open Windows default audio output");
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = rate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = 4;
        format.nAvgBytesPerSec = rate * format.nBlockAlign;
        volume = requested_volume;
        for (auto& slot : slots) slot.samples.reserve(8192 * 2);
        create_source();
        active = true;
    }
    void set_active(bool value) {
        if (!engine || active == value) return;
        destroy_source();
        active = value;
        if (active) create_source();
    }
    void submit(const std::vector<std::int16_t>& samples) {
        if (!source || !active || samples.empty()) return;
        check(callback.error.load(), "Audio device/voice");
        if (samples.size() % 2 || samples.size() > 8192 * 2) throw std::invalid_argument("Invalid stereo PCM block");
        Slot* available = nullptr;
        for (auto& slot : slots) {
            bool expected = false;
            if (slot.busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) { available = &slot; break; }
        }
        if (!available) {
            // Recover from excessive latency by dropping stale queued audio.
            destroy_source();
            create_source();
            available = &slots[0];
            available->busy.store(true);
        }
        try {
            available->samples.assign(samples.begin(), samples.end());
            XAUDIO2_BUFFER buffer{};
            buffer.AudioBytes = static_cast<UINT32>(samples.size() * sizeof(std::int16_t));
            buffer.pAudioData = reinterpret_cast<const BYTE*>(available->samples.data());
            buffer.pContext = available;
            check(source->SubmitSourceBuffer(&buffer), "Queue game audio");
        } catch (...) {
            available->busy.store(false, std::memory_order_release);
            throw;
        }
        if (!started) {
            XAUDIO2_VOICE_STATE state{};
            source->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (state.BuffersQueued >= 2) {
                check(source->Start(), "Start game audio");
                started = true;
            }
        }
    }
};
AudioOutput::AudioOutput() : impl(std::make_unique<Impl>()) {}
AudioOutput::~AudioOutput() = default;
void AudioOutput::initialize(unsigned rate, float volume) { impl->initialize(rate, volume); }
void AudioOutput::set_active(bool active) { impl->set_active(active); }
void AudioOutput::set_muted(bool muted) {
    impl->muted = muted;
    if (impl->source) check(impl->source->SetVolume(muted ? 0 : impl->volume), "Mute game audio");
}
void AudioOutput::submit(const std::vector<std::int16_t>& samples) { impl->submit(samples); }
void AudioOutput::set_volume(float volume) {
    if (!std::isfinite(volume) || volume < 0 || volume > 1) throw std::invalid_argument("Invalid audio volume");
    if (impl->volume == volume) return;
    impl->volume = volume;
    if (impl->source) check(impl->source->SetVolume(impl->muted ? 0 : volume), "Set game audio volume");
}
}
