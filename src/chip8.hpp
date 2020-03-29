#pragma once

#include "chip8_emulator.hpp"

namespace chip8 {

void init(const char* rom_path, bool show_debug_panel);
void terminate(void);
void run(void);

} // namespace chip8
