#pragma once

#include <atomic>
#include <cstdint>

namespace chip8::glfw {

void init(void);
void terminate(void);
void draw_screen(void);
void poll_user_input(void);

uint64_t display_width_pixels(void);
uint64_t display_height_pixels(void);
uint64_t display_refresh_rate(void);

std::atomic<bool>& shutdown_flag(void);

} // namespace chip8::glfw
