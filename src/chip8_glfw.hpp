#pragma once

namespace chip8::glfw {

void init(void) {}
void terminate(void) {}
void draw_screen(const struct emulator& emu) {}
void update_input_state(const struct emulator& emu) {}

}  // namespace chip8::glfw

