#pragma once

#include "chip8_emulator.hpp"

namespace chip8::audio {

void init(void);
void terminate(void);

void start_beep(void);
void stop_beep(void);

} // namespace chip8::audio