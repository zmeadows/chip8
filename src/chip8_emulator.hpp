#pragma once

#include "chip8_flag.hpp"

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

chip8::sync_flag& draw_flag(void);
chip8::sync_flag& beep_flag(void);

void init(const char* rom_path);
void terminate(void);

void emulate_cycle(void);

void copy_screen_state(bool* buffer);

void update_user_input(const bool* const new_input);

void decrement_timers(void);

template <typename Callable>
void for_each_instr_in_history(Callable&& f);

} // namespace chip8::emulator
