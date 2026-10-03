// SPDX-License-Identifier: GPL-2.0-or-later
#include "openxr_session.h"
#include "desktop_window.h"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>

namespace {
struct Options {
    bvb::ScreenSettings screen;
    double seconds = 0;
    bool preview_only = false;
    bool help = false;
    std::string export_path;
};
double number(const char* value) {
    std::size_t consumed = 0;
    const std::string text(value);
    double result = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(result))
        throw std::invalid_argument("Expected a finite numeric value: " + text);
    return result;
}
Options parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        std::string flag(argv[i]);
        if (flag == "--help" || flag == "-h") options.help = true;
        else if (flag == "--swap-eyes") options.screen.swap_eyes = true;
        else if (flag == "--preview-only") options.preview_only = true;
        else if (flag == "--width" || flag == "--distance" || flag == "--seconds" || flag == "--export-pattern") {
            if (++i >= argc) throw std::invalid_argument("Missing value for " + flag);
            if (flag == "--export-pattern") options.export_path = argv[i];
            else {
                double value = number(argv[i]);
                if (flag == "--width") {
                    if (value < 0.3 || value > 3.0) throw std::invalid_argument("Width must be 0.3 to 3.0 metres");
                    options.screen.width = static_cast<float>(value);
                } else if (flag == "--distance") {
                    if (value < 0.5 || value > 5.0) throw std::invalid_argument("Distance must be 0.5 to 5.0 metres");
                    options.screen.distance = static_cast<float>(value);
                } else {
                    if (value < 0) throw std::invalid_argument("Seconds must be nonnegative; zero means unlimited");
                    options.seconds = value;
                }
            }
        } else throw std::invalid_argument("Unknown option: " + flag);
    }
    return options;
}
void help() {
    std::cout << "Beetle VB OpenXR stereo screen test (no game emulation)\n"
        "Usage: bvb_stereo_test [--width metres] [--distance metres] [--swap-eyes]\n"
        "                       [--seconds duration] [--preview-only]\n"
        "                       [--export-pattern file.bmp] [--help]\n"
        "Default width 1.2m, distance 2m, duration unlimited.\n"
        "Desktop window keys: R recenter, S swap eyes, +/- width, [/] distance, Esc exit.\n"
        "--preview-only opens the pattern window without starting OpenXR.\n"
        "--export-pattern writes a side-by-side BMP and exits without starting OpenXR.\n";
}
std::vector<std::uint8_t> preview_pixels(const bvb::StereoFrame& frame, bool swap) {
    const auto left = bvb::rgba_to_bgra(swap ? frame.right : frame.left);
    const auto right = bvb::rgba_to_bgra(swap ? frame.left : frame.right);
    std::vector<std::uint8_t> pixels(bvb::eye_width * 2 * bvb::eye_height * 4);
    for (unsigned y = 0; y < bvb::eye_height; ++y) {
        auto* row = pixels.data() + y * bvb::eye_width * 8;
        std::copy_n(left.data() + y * bvb::eye_width * 4, bvb::eye_width * 4, row);
        std::copy_n(right.data() + y * bvb::eye_width * 4, bvb::eye_width * 4, row + bvb::eye_width * 4);
    }
    return pixels;
}
void export_pattern(const std::string& path, const bvb::StereoFrame& frame, bool swap) {
    const auto pixels = preview_pixels(frame, swap);
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER image{};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(image);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    image.biSize = sizeof(image);
    image.biWidth = bvb::eye_width * 2;
    image.biHeight = -int(bvb::eye_height);
    image.biPlanes = 1;
    image.biBitCount = 32;
    image.biCompression = BI_RGB;
    image.biSizeImage = static_cast<DWORD>(pixels.size());
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&file), sizeof(file));
    output.write(reinterpret_cast<const char*>(&image), sizeof(image));
    output.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    if (!output) throw std::runtime_error("Cannot export pattern: " + path);
    std::cout << "Wrote 768x224 side-by-side diagnostic: " << path << '\n';
}

volatile LONG interrupted = 0;
BOOL WINAPI console_control(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        InterlockedExchange(&interrupted, 1);
        return TRUE;
    }
    return FALSE;
}
}

int main(int argc, char** argv) {
    try {
        auto options = parse(argc, argv);
        if (options.help) { help(); return 0; }
        const auto pattern = bvb::make_test_pattern();
        if (!options.export_path.empty()) {
            export_pattern(options.export_path, pattern, options.screen.swap_eyes);
            return 0;
        }
        help();
        SetConsoleCtrlHandler(console_control, TRUE);
        struct ConsoleCleanup { ~ConsoleCleanup() { SetConsoleCtrlHandler(console_control, FALSE); } } console_cleanup;
        bvb::DesktopWindow preview(pattern, options.screen);
        bvb::OpenXrSession xr;
        if (!options.preview_only) xr.initialize();
        const auto started = std::chrono::steady_clock::now();
        bool exiting = false;
        auto exit_time = started;
        for (;;) {
            preview.pump();
            preview.refresh();
            const auto now = std::chrono::steady_clock::now();
            const bool timed_out = options.seconds > 0 && std::chrono::duration<double>(now - started).count() >= options.seconds;
            const bool quit = preview.close_requested || InterlockedCompareExchange(&interrupted, 0, 0) || timed_out;
            if (options.preview_only) {
                if (quit) break;
                Sleep(10);
                continue;
            }
            xr.poll_events();
            if (xr.finished()) break;
            if (quit && !exiting) {
                xr.request_exit();
                exiting = true;
                exit_time = now;
            }
            if (preview.recenter_requested) {
                xr.recenter();
                preview.recenter_requested = false;
            }
            if (xr.finished()) break;
            if (exiting && now - exit_time > std::chrono::seconds(5)) {
                std::cerr << "Runtime did not stop within five seconds; destroying session.\n";
                break;
            }
            if (xr.running()) xr.render_frame(pattern, options.screen);
            else Sleep(10);
        }
        std::cout << "Stereo test closed.\n";
        return xr.lost() ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nFor VR, connect the headset and start the active OpenXR runtime.\n"
                  << "Use --preview-only or --export-pattern file.bmp to inspect the pattern without VR.\n";
        return 1;
    }
}
