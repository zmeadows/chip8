#pragma once

#include <cstdint>

namespace chip8::emulator {

constexpr auto memory_size_bytes = 4096;
constexpr auto rom_memory_offset = 0x200;
constexpr auto allowed_rom_memory = memory_size_bytes - rom_memory_offset;
constexpr auto display_grid_width = 64;
constexpr auto display_grid_height = 32;
constexpr auto pixel_count = display_grid_width * display_grid_height;
constexpr auto max_stack_depth = 16;
constexpr auto user_input_key_count = 16;
constexpr auto register_count = 16;

void init(const char* rom_path);
void terminate(void);

void emulate_cycle(void);
const bool* const get_screen_state(void);
void update_user_input(uint8_t key_id, bool new_state);

bool is_beeping(void);

} // namespace chip8::emulator
