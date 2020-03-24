#pragma once

#include "chip8_core.hpp"

namespace chip8 {

void init(void);
void terminate(void);

struct chip8::core::emulator create_emulator(const char* rom_path);

bool tick(struct chip8::core::emulator& emu);

} // namespace chip8
