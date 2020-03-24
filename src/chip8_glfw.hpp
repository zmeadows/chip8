#pragma once

#include "chip8_core.hpp"

namespace chip8::glfw {

void init(void);
void terminate(void);
void draw_screen(const struct chip8::core::emulator& emu);
void update_input_state(const struct chip8::core::emulator& emu);
bool user_requested_window_close(void);

} // namespace chip8::glfw
