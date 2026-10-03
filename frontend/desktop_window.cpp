// SPDX-License-Identifier: GPL-2.0-or-later
#include "desktop_window.h"
#include <windows.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace bvb {
struct DesktopWindow::Impl {
    DesktopWindow& owner;
    const StereoFrame& frame;
    ScreenSettings& screen;
    bool player, last_swap = false, effective_paused = false;
    std::uint64_t last_frame = std::numeric_limits<std::uint64_t>::max();
    std::vector<std::uint8_t> pixels;
    HWND window = nullptr;
    Impl(DesktopWindow& owner, const StereoFrame& frame, ScreenSettings& screen, bool player)
        : owner(owner), frame(frame), screen(screen), player(player) {}
    ~Impl() { if (window) DestroyWindow(window); }
    void update_pixels() {
        pixels=stereo_preview_pixels(frame,screen.swap_eyes);
    }
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM key, LPARAM value) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(value)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd, message, key, value);
        switch (message) {
        case WM_CLOSE: self->owner.close_requested = true; return 0;
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: InvalidateRect(hwnd, nullptr, FALSE); return 0;
        case WM_GETMINMAXINFO:
            reinterpret_cast<MINMAXINFO*>(value)->ptMinTrackSize = {640, 320}; return 0;
        case WM_KEYDOWN: {
            const bool repeated = (value & (1ll << 30)) != 0;
            if (self->player && !repeated && (key == VK_ESCAPE || key == VK_F3)) self->owner.menu_requested = true;
            else if (!self->player && key == VK_ESCAPE) self->owner.close_requested = true;
            else if (!repeated && key == (self->player ? VK_F1 : 'R')) self->owner.recenter_requested = true;
            else if (!repeated && key == (self->player ? VK_F2 : 'S')) self->screen.swap_eyes = !self->screen.swap_eyes;
            else if (self->player && !repeated && key == 'P') self->owner.paused = !self->owner.paused;
            else if (self->player && !repeated && key == 'M') self->owner.muted = !self->owner.muted;
            else if (key == VK_OEM_PLUS || key == VK_ADD) self->screen.width += 0.1f;
            else if (key == VK_OEM_MINUS || key == VK_SUBTRACT) self->screen.width -= 0.1f;
            else if (key == VK_OEM_4) self->screen.distance -= 0.1f;
            else if (key == VK_OEM_6) self->screen.distance += 0.1f;
            else return DefWindowProcW(hwnd, message, key, value);
            self->screen.clamp();
            // Pixel allocation happens in the caller's refresh(), outside WndProc.
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            RECT client{}; GetClientRect(hwnd, &client);
            FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(240,240,240));
            RECT instructions{16, 12, client.right - 16, 74};
            const wchar_t* label = self->player
                ? L"WASD: left pad   Arrows: right pad   J/K: B/A   Q/E: L/R   Enter/Space: Start/Select\nF3 / Esc: settings menu   Gamepad Back+Start: menu   F1: recenter   F2: swap eyes   P: pause   M: mute"
                : L"Stereo test image preview\nR: recenter   S: swap eyes   +/-: screen width   [ / ]: distance   Esc: exit";
            DrawTextW(dc, label, -1, &instructions, DT_LEFT | DT_TOP | DT_WORDBREAK);
            const int available_width = std::max(1L, client.right - 32);
            const int available_height = std::max(1L, client.bottom - 116);
            const float scale = std::min(float(available_width) / (self->frame.width * 2), float(available_height) / self->frame.height);
            const int width = int(self->frame.width * 2 * scale), height = int(self->frame.height * scale);
            BITMAPINFO bitmap{};
            bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bitmap.bmiHeader.biWidth = self->frame.width * 2; bitmap.bmiHeader.biHeight = -int(self->frame.height);
            bitmap.bmiHeader.biPlanes = 1; bitmap.bmiHeader.biBitCount = 32; bitmap.bmiHeader.biCompression = BI_RGB;
            SetStretchBltMode(dc, self->frame.width > eye_width ? HALFTONE : COLORONCOLOR);
            SetBrushOrgEx(dc,0,0,nullptr);
            if (!self->pixels.empty()) StretchDIBits(dc, (client.right - width) / 2, 76 + (available_height - height) / 2,
                width, height, 0, 0, self->frame.width * 2, self->frame.height, self->pixels.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
            wchar_t status[192]{};
            swprintf_s(status, L"Width %.1fm   Distance %.1fm   Eyes %ls   %ls   %ls   (source image preview)",
                self->screen.width, self->screen.distance, self->screen.swap_eyes ? L"swapped" : L"normal",
                self->player && self->effective_paused ? L"PAUSED" : L"", self->owner.muted ? L"MUTED" : L"");
            RECT footer{16, client.bottom - 30, client.right - 16, client.bottom};
            DrawTextW(dc, status, -1, &footer, DT_LEFT | DT_TOP);
            EndPaint(hwnd, &paint); return 0;
        }
        }
        return DefWindowProcW(hwnd, message, key, value);
    }
    void create(const std::wstring& title) {
        update_pixels();
        WNDCLASSW type{};
        type.lpfnWndProc = procedure; type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"BeetleVBDesktopPreview"; type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Cannot register preview window class");
        window = CreateWindowExW(0, type.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 1024, 440, nullptr, nullptr, type.hInstance, this);
        if (!window) throw std::runtime_error("Cannot create preview window");
        ShowWindow(window, SW_SHOW); UpdateWindow(window);
    }
};
DesktopWindow::DesktopWindow(const StereoFrame& frame, ScreenSettings& screen, bool player, const std::wstring& title)
    : impl(std::make_unique<Impl>(*this, frame, screen, player)) { impl->create(title); }
DesktopWindow::~DesktopWindow() = default;
void DesktopWindow::pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) close_requested = true;
        TranslateMessage(&message); DispatchMessageW(&message);
    }
}
bool DesktopWindow::foreground() const { return GetForegroundWindow() == impl->window; }
void DesktopWindow::refresh(std::uint64_t frame_number, bool is_paused) {
    bool changed = frame_number != impl->last_frame || impl->last_swap != impl->screen.swap_eyes;
    if (changed) impl->update_pixels();
    if (changed || is_paused != impl->effective_paused) InvalidateRect(impl->window, nullptr, FALSE);
    impl->last_frame = frame_number; impl->last_swap = impl->screen.swap_eyes; impl->effective_paused = is_paused;
}
}
