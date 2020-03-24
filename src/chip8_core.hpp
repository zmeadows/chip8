#pragma once

#include <chrono>

namespace chip8 {
using clock = std::chrono::high_resolution_clock;
}

namespace chip8::core {

// see https://en.wikipedia.org/wiki/CHIP-8#Virtual_machine_description
struct emulator {
    static constexpr auto memory_size_bytes = 4096;
    static constexpr auto rom_memory_offset = 0x200;
    static constexpr auto allowed_rom_memory = memory_size_bytes - rom_memory_offset;
    static constexpr auto screen_width = 64;
    static constexpr auto screen_height = 32;
    static constexpr auto pixel_count = screen_width * screen_height;
    static constexpr auto max_stack_depth = 16;
    static constexpr auto user_input_key_count = 16;
    static constexpr auto register_count = 16;

    uint8_t memory[memory_size_bytes];
    bool gfx[screen_width * screen_height];
    char last_instruction[256];
    uint16_t stack_trace[max_stack_depth];
    uint8_t V[register_count];
    uint8_t input[user_input_key_count];
    uint16_t idx; // index register
    uint16_t pc;  // program counter
    uint16_t sp;  // stack "pointer"
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool draw_flag;
    uint64_t cycles_emulated;

    chip8::clock::time_point last_cycle;
};

struct emulator create_emulator(const char* rom_path);
void emulate_cycle(struct emulator& emu);

} // namespace chip8::core
