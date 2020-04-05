#pragma once

#include <cstdint>

namespace chip8::emulator {

void init(const char* rom_path);
void terminate(void);

void emulate_cycle(void);
const bool* const get_screen_state(void);
void update_user_input(uint8_t key_id, bool new_state);

bool is_beeping(void);

} // namespace chip8::emulator
