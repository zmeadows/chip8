#pragma once

#include <chrono>
using namespace std::chrono;

namespace chip8 {

constexpr auto memory_size_bytes = 4096;
constexpr auto rom_memory_offset = 0x200;
constexpr auto allowed_rom_memory = memory_size_bytes - rom_memory_offset;
constexpr auto maximum_rom_instruction_count = allowed_rom_memory / 2;
constexpr auto display_grid_width = 64;
constexpr auto display_grid_height = 32;
constexpr auto pixel_count = display_grid_width * display_grid_height;
constexpr auto max_stack_depth = 16;
constexpr auto user_input_key_count = 16;
constexpr auto register_count = 16;

using clock =
    std::conditional<high_resolution_clock::is_steady, high_resolution_clock,
                     std::conditional<system_clock::period::den <= steady_clock::period::den,
                                      system_clock, steady_clock>::type>::type;

template <uint16_t index>
constexpr uint16_t ith_hex_digit(uint16_t opcode)
{
    static_assert(index >= 0 && index <= 3);
    constexpr uint16_t offset = 12 - index * 4;
    constexpr uint16_t mask = 0x000F << offset;
    return (mask & opcode) >> offset;
}

inline void panic_opcode(const char* description, const uint16_t opcode)
{
    printf("%s CHIP8 op-code encountered: 0x%04X\n", description, opcode);
    exit(1);
};


} // namespace chip8