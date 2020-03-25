#pragma once

#include <chrono>
#include <deque>
#include <optional>
#include <string>

namespace chip8 {

namespace {

using system_clock = std::chrono::system_clock;
using steady_clock = std::chrono::steady_clock;
using high_resolution_clock = std::chrono::high_resolution_clock;

using maxres_sys_or_steady_clock =
    std::conditional<system_clock::period::den <= steady_clock::period::den, system_clock,
                     steady_clock>::type;
} // namespace

using clock = std::conditional<high_resolution_clock::is_steady, high_resolution_clock,
                               maxres_sys_or_steady_clock>::type;
} // namespace chip8

namespace chip8::core {

// see https://en.wikipedia.org/wiki/CHIP-8#Virtual_machine_description
struct emulator {
    static constexpr auto memory_size_bytes = 4096;
    static constexpr auto rom_memory_offset = 0x200;
    static constexpr auto allowed_rom_memory = memory_size_bytes - rom_memory_offset;
    static constexpr auto display_grid_width = 64;
    static constexpr auto display_grid_height = 32;
    static constexpr auto pixel_count = display_grid_width * display_grid_height;
    static constexpr auto max_stack_depth = 16;
    static constexpr auto user_input_key_count = 16;
    static constexpr auto register_count = 16;

    uint8_t memory[memory_size_bytes];
    bool gfx[pixel_count];
    uint16_t stack_trace[max_stack_depth];
    uint8_t V[register_count];
    bool input[user_input_key_count];

    uint16_t idx; // index register
    uint16_t pc;  // program counter
    uint16_t sp;  // stack "pointer"

    uint8_t delay_timer;
    uint8_t sound_timer;

    std::optional<uint16_t> register_awaiting_input;

    bool draw_flag;

    chip8::clock::time_point last_cycle;
    uint64_t cycles_emulated;

    std::deque<std::string> instr_history;
    uint64_t history_size;
};

struct emulator create_emulator(const char* rom_path);
void emulate_cycle(struct emulator& emu);
void update_user_input(struct emulator& emu,
                       const bool new_input[emulator::user_input_key_count]);

} // namespace chip8::core
