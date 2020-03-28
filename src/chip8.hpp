#pragma once

#include "chip8_emulator.hpp"

namespace chip8 {

void init(const char* rom_path);
void terminate(void);
void run(void);

} // namespace chip8
