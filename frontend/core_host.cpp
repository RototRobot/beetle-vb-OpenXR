// SPDX-License-Identifier: GPL-2.0-or-later
#include "core_host.h"
#include "save_ram.h"
#include "state_file.h"
#include <libretro.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace bvb {
struct CoreHost::Impl {
    static Impl* active;
#ifdef _WIN32
    HMODULE module = nullptr;
#else
    void* module = nullptr;
#endif
    bool initialized = false, loaded = false, saves_enabled = false, xrgb = false;
    std::map<std::string, std::string> options;
    bool variables_updated = false;
    std::string system_directory, save_directory, library_name, content_path;
    std::filesystem::path save_file;
    std::filesystem::path library_file;
    std::string state_core_id;
    std::vector<std::uint8_t> content;
    StereoFrame video_frame;
    std::vector<std::int16_t> pcm;
    std::uint16_t input_mask = 0;
    std::uint64_t count = 0;
    double frame_rate = 0;
    unsigned audio_rate = 0;
    std::exception_ptr callback_error;
    decltype(&retro_deinit) deinit = nullptr;
    decltype(&retro_unload_game) unload = nullptr;
    decltype(&retro_load_game) load = nullptr;
    decltype(&retro_run) run = nullptr;
    decltype(&retro_get_system_av_info) av_info = nullptr;
    decltype(&retro_get_memory_data) memory_data = nullptr;
    decltype(&retro_get_memory_size) memory_size = nullptr;
    decltype(&retro_serialize_size) serialize_size = nullptr;
    decltype(&retro_serialize) serialize = nullptr;
    decltype(&retro_unserialize) unserialize = nullptr;
    ~Impl() {
        if (loaded) {
            try { save_ram(); } catch (const std::exception& error) { std::cerr << "Save RAM error: " << error.what() << '\n'; }
            unload();
        }
        if (initialized) deinit();
        if (active == this) active = nullptr;
        if (module) {
#ifdef _WIN32
            FreeLibrary(module);
#else
            dlclose(module);
#endif
        }
    }
    template<class T> T symbol(const char* name) {
#ifdef _WIN32
        auto address = GetProcAddress(module, name);
#else
        auto address = dlsym(module, name);
#endif
        if (!address) throw std::runtime_error(std::string("Core symbol missing: ") + name);
        return reinterpret_cast<T>(address);
    }
    template<class F> static void callback(F function) noexcept {
        if (!active || active->callback_error) return;
        try { function(*active); } catch (...) { active->callback_error = std::current_exception(); }
    }
    void check_callbacks() {
        if (callback_error) { auto error = callback_error; callback_error = nullptr; std::rethrow_exception(error); }
    }
    static void log_message(retro_log_level, const char* format, ...) {
        va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
    }
    static bool environment(unsigned command, void* data) noexcept {
        bool result = false;
        callback([&](Impl& self) { result = self.environment_impl(command, data); });
        return result;
    }
    bool environment_impl(unsigned command, void* data) {
        switch (command) {
        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: *static_cast<unsigned*>(data) = 0; return true;
        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            auto* vars = static_cast<retro_variable*>(data);
            for (; vars->key; ++vars) {
                std::string value = vars->value ? vars->value : "";
                auto start = value.find("; ");
                if (start != std::string::npos) {
                    value = value.substr(start + 2);
                    options[vars->key] = value.substr(0, value.find('|'));
                }
            }
            options["vb_3dmode"] = "side-by-side";
            options["vb_sidebyside_separation"] = "0";
            options["vb_right_analog_to_digital"] = "disabled";
            return true;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            auto* variable = static_cast<retro_variable*>(data);
            auto found = options.find(variable->key);
            variable->value = found == options.end() ? nullptr : found->second.c_str();
            return variable->value != nullptr;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *static_cast<bool*>(data) = variables_updated; variables_updated = false; return true;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            xrgb = *static_cast<retro_pixel_format*>(data) == RETRO_PIXEL_FORMAT_XRGB8888; return xrgb;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: static_cast<retro_log_callback*>(data)->log = log_message; return true;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: *static_cast<const char**>(data) = system_directory.c_str(); return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *static_cast<const char**>(data) = save_directory.c_str(); return true;
        case RETRO_ENVIRONMENT_GET_CAN_DUPE: *static_cast<bool*>(data) = true; return true;
        case RETRO_ENVIRONMENT_GET_OVERSCAN: *static_cast<bool*>(data) = false; return true;
        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: return true;
        case RETRO_ENVIRONMENT_SET_GEOMETRY:
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS: return true;
        default: return false;
        }
    }
    static void video(const void* data, unsigned width, unsigned height, std::size_t pitch) noexcept {
        callback([&](Impl& self) {
            if (!data) return;
            if (data == RETRO_HW_FRAME_BUFFER_VALID || !self.xrgb || !copy_stereo_frame(data, width, height, pitch, self.video_frame))
                throw std::runtime_error("Expected a 768x224 side-by-side XRGB8888 core frame");
        });
    }
    static void audio_sample(std::int16_t left, std::int16_t right) noexcept {
        callback([&](Impl& self) { self.pcm.push_back(left); self.pcm.push_back(right); });
    }
    static std::size_t audio_batch(const std::int16_t* samples, std::size_t frames) noexcept {
        callback([&](Impl& self) {
            if (!frames) return;
            if (!samples || frames > 8192) throw std::runtime_error("Invalid core audio batch");
            self.pcm.insert(self.pcm.end(), samples, samples + frames * 2);
        });
        return frames;
    }
    static void input_poll() noexcept {}
    static std::int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id) noexcept {
        if (!active || port || device != RETRO_DEVICE_JOYPAD || index) return 0;
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return static_cast<std::int16_t>(active->input_mask);
        return id < 16 && (active->input_mask & (1u << id)) ? 1 : 0;
    }
    void initialize(const std::filesystem::path& library) {
        if (module || active) throw std::logic_error("Only one active core host is supported per process");
#ifdef _WIN32
        module = LoadLibraryW(std::filesystem::absolute(library).c_str());
#else
        module = dlopen(std::filesystem::absolute(library).c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
        if (!module) throw std::runtime_error("Cannot load core library: " + library.u8string());
#define CORE(name) symbol<decltype(&name)>(#name)
        deinit = CORE(retro_deinit); unload = CORE(retro_unload_game); load = CORE(retro_load_game); run = CORE(retro_run);
        av_info = CORE(retro_get_system_av_info); memory_data = CORE(retro_get_memory_data); memory_size = CORE(retro_get_memory_size);
        serialize_size = CORE(retro_serialize_size); serialize = CORE(retro_serialize); unserialize = CORE(retro_unserialize);
        library_file = std::filesystem::absolute(library);
        if (CORE(retro_api_version)() != RETRO_API_VERSION) throw std::runtime_error("Libretro API mismatch");
        system_directory = std::filesystem::absolute(library).parent_path().u8string();
        save_directory = system_directory;
        active = this;
        CORE(retro_set_environment)(environment); CORE(retro_set_video_refresh)(video);
        CORE(retro_set_audio_sample)(audio_sample); CORE(retro_set_audio_sample_batch)(audio_batch);
        CORE(retro_set_input_poll)(input_poll); CORE(retro_set_input_state)(input_state);
        retro_system_info info{}; CORE(retro_get_system_info)(&info);
        if (info.need_fullpath) throw std::runtime_error("This host expects a memory-loaded Beetle VB core");
        library_name = std::string(info.library_name ? info.library_name : "Unknown") + " " + (info.library_version ? info.library_version : "");
        auto init = CORE(retro_init); initialized = true; init(); check_callbacks();
        CORE(retro_set_controller_port_device)(0, RETRO_DEVICE_JOYPAD);
#undef CORE
    }
    void load_content() {
        if (!initialized || loaded) throw std::logic_error("Initialize an empty core host before loading content");
        if (content.size() < 256 || content.size() > (1u << 24) || (content.size() & (content.size() - 1)))
            throw std::runtime_error("Virtual Boy ROM must have a power-of-two size, from 256 bytes to 16 MiB");
        retro_game_info game{content_path.c_str(), content.data(), content.size(), nullptr};
        if (!load(&game)) { check_callbacks(); throw std::runtime_error("Core rejected cartridge"); }
        loaded = true; check_callbacks();
        retro_system_av_info info{}; av_info(&info);
        if (!std::isfinite(info.timing.fps) || info.timing.fps < 1 || info.timing.fps > 1000 ||
            !std::isfinite(info.timing.sample_rate) || info.timing.sample_rate < 8000 || info.timing.sample_rate > 192000)
            throw std::runtime_error("Core reported invalid frame/audio timing");
        frame_rate = info.timing.fps; audio_rate = static_cast<unsigned>(std::lround(info.timing.sample_rate));
        if (!save_file.empty()) {
            read_save_ram(save_file, memory_data(RETRO_MEMORY_SAVE_RAM), memory_size(RETRO_MEMORY_SAVE_RAM));
            saves_enabled = true; // Failed validation must not overwrite an existing save.
        }
    }
    void save_ram() {
        if (loaded && saves_enabled) write_save_ram(save_file, memory_data(RETRO_MEMORY_SAVE_RAM), memory_size(RETRO_MEMORY_SAVE_RAM));
    }
    std::string core_id() {
        if (!state_core_id.empty()) return state_core_id;
        const auto size = std::filesystem::file_size(library_file);
        if (!size || size > 64 * 1024 * 1024) throw std::runtime_error("Invalid core library size for save states");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        std::ifstream input(library_file, std::ios::binary);
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input || input.peek() != std::char_traits<char>::eof()) throw std::runtime_error("Cannot identify core build");
        state_core_id = cartridge_id(bytes); return state_core_id;
    }
    std::size_t state_size() {
        if (!loaded) throw std::logic_error("No cartridge loaded");
        const auto size = serialize_size(); check_callbacks();
        if (!size || size > max_state_size) throw std::runtime_error("Core does not provide a supported save state");
        return size;
    }
    std::vector<std::uint8_t> capture_state() {
        std::vector<std::uint8_t> bytes(state_size());
        const bool result = serialize(bytes.data(),bytes.size()); check_callbacks();
        if (!result) throw std::runtime_error("Core could not save its state");
        return bytes;
    }
};
CoreHost::Impl* CoreHost::Impl::active = nullptr;
CoreHost::CoreHost() : impl(std::make_unique<Impl>()) {}
CoreHost::~CoreHost() = default;
void CoreHost::initialize(const std::filesystem::path& library) { impl->initialize(library); }
void CoreHost::load_game(const std::filesystem::path& rom, const std::filesystem::path& saves) {
    if (!impl->initialized || impl->loaded) throw std::logic_error("Core host is not ready to load a game");
    auto size = std::filesystem::file_size(rom);
    if (size < 256 || size > (1u << 24) || (size & (size - 1)))
        throw std::runtime_error("Virtual Boy ROM must have a power-of-two size, from 256 bytes to 16 MiB");
    impl->content.resize(static_cast<std::size_t>(size));
    std::ifstream input(rom, std::ios::binary);
    input.read(reinterpret_cast<char*>(impl->content.data()), static_cast<std::streamsize>(size));
    if (!input || input.peek() != std::char_traits<char>::eof()) throw std::runtime_error("Cannot read complete ROM: " + rom.u8string());
    impl->content_path = std::filesystem::absolute(rom).u8string();
    if (saves.empty()) impl->save_file.clear();
    else {
        impl->save_directory = std::filesystem::absolute(saves).u8string();
        impl->save_file = std::filesystem::absolute(saves) / (cartridge_id(impl->content) + ".srm");
    }
    impl->load_content();
}
void CoreHost::load_synthetic() {
    if (!impl->initialized || impl->loaded) throw std::logic_error("Core host is not ready to load synthetic content");
    impl->content.assign(65536, 0); impl->content_path = "synthetic-blank.vb"; impl->save_file.clear(); impl->load_content();
}
void CoreHost::run_frame(const GameInput& input) {
    if (!impl->loaded) throw std::logic_error("No cartridge loaded");
    impl->input_mask = libretro_buttons(input); impl->pcm.clear(); impl->run(); impl->check_callbacks(); ++impl->count;
}
void CoreHost::save_ram() { impl->save_ram(); }
void CoreHost::save_state(const std::filesystem::path& file) {
    const auto bytes = impl->capture_state();
    write_state_file(file, {cartridge_key(),impl->core_id(),bytes,impl->video_frame});
}
void CoreHost::load_state(const std::filesystem::path& file) {
    auto saved = read_state_file(file,cartridge_key(),impl->core_id(),impl->state_size());
    const auto backup = impl->capture_state();
    bool restored = false;
    try { restored = impl->unserialize(saved.payload.data(),saved.payload.size()); impl->check_callbacks(); }
    catch (...) { restored = false; }
    if (!restored) {
        bool recovered = false;
        try { recovered = impl->unserialize(backup.data(),backup.size()); impl->check_callbacks(); } catch (...) {}
        if (!recovered) {
            impl->saves_enabled = false;
            throw StateRecoveryError("State recovery failed; player must close without saving cartridge RAM");
        }
        throw std::runtime_error("Core rejected save state; current game restored");
    }
    impl->video_frame = std::move(saved.frame); impl->pcm.clear(); impl->input_mask = 0;
    impl->variables_updated = true; // Keep current presentation options after restore.
}
std::string CoreHost::cartridge_key() const {
    if (!impl->loaded) throw std::logic_error("No cartridge loaded");
    return cartridge_id(impl->content);
}
void CoreHost::set_palette(const std::string& palette) {
    const char* valid[] = {"black & red", "black & white", "black & blue", "black & cyan",
        "black & electric cyan", "black & green", "black & magenta", "black & yellow"};
    bool found = false;
    for (const auto* value : valid) if (palette == value) found = true;
    if (!found) throw std::invalid_argument("Unknown Virtual Boy palette");
    if (!impl->initialized) throw std::logic_error("Initialize the core before setting its palette");
    if (impl->options["vb_color_mode"] != palette) {
        impl->options["vb_color_mode"] = palette; impl->variables_updated = true;
    }
}
const StereoFrame& CoreHost::frame() const { return impl->video_frame; }
const std::vector<std::int16_t>& CoreHost::audio() const { return impl->pcm; }
double CoreHost::fps() const { return impl->frame_rate; }
unsigned CoreHost::sample_rate() const { return impl->audio_rate; }
std::uint64_t CoreHost::frame_count() const { return impl->count; }
std::string CoreHost::name() const { return impl->library_name; }
std::filesystem::path CoreHost::save_path() const { return impl->save_file; }
}
