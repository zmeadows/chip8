#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "chip8.hpp"

namespace {

constexpr bool CHIP8_DEBUG = false;

void debug_print(const char* fmt, ...)
{
    if constexpr (CHIP8_DEBUG) {
        va_list argptr;
        va_start(argptr, fmt);
        vfprintf(stderr, fmt, argptr);
        va_end(argptr);
    }
}

inline void panic_opcode(const char* description, const uint16_t opcode)
{
    printf("%s CHIP8 op-code encountered: 0x%04X\n", description, opcode);
    exit(1);
};

template <uint16_t index>
constexpr uint16_t ith_hex_digit(uint16_t opcode)
{
    static_assert(index >= 0 && index <= 3);
    constexpr uint16_t offset = 12 - index * 4;
    constexpr uint16_t mask = 0x000F << offset;
    return (mask & opcode) >> offset;
}

void reset(struct emulator& emu)
{
    static constexpr uint8_t chip8_fontset[80] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
        0x20, 0x60, 0x20, 0x20, 0x70,  // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
        0xF0, 0x80, 0xF0, 0x80, 0x80   // F
    };

    for (auto i = 0; i < 80; i++) {
        emu.memory[i] = chip8_fontset[i];
    }

    for (auto i = 80; i < emulator::memory_size_bytes; i++) {
        emu.memory[i] = 0;
    }

    for (auto i = 0; i < emulator::screen_width * emulator::screen_height; i++) {
        emu.gfx[i] = false;
    }

    for (auto i = 0; i < emulator::max_stack_depth; i++) {
        emu.stack_trace[i] = 0;
    }

    for (auto i = 0; i < emulator::register_count; i++) {
        emu.V[i] = 0;
    }

    for (auto i = 0; i < emulator::user_input_key_count; i++) {
        emu.input[i] = 0;
    }

    emu.idx = 0;
    emu.pc = 0x200;
    emu.sp = 0;
    emu.delay_timer = 0;
    emu.sound_timer = 0;
    emu.draw_flag = false;
    emu.cycles_emulated = 0;
    emu.last_cycle = chip8::core::clock::now();
}

void emulate_0x0NNN_opcode_cycle(struct emulator& emu, const uint16_t opcode, bool& bump_pc)
{
    assert((opcode & 0xF000) == 0x0000);

    switch (opcode) {
        case 0x00E0: {  // clear screen
            for (auto i = 0; i < emulator::screen_width * emulator::screen_height; i++) {
                emu.gfx[i] = false;
            }
            emu.draw_flag = true;
            break;
        }
        case 0x00EE: {  // return from a subroutine
            assert(emu.sp > 0);
            emu.sp--;
            emu.pc = emu.stack_trace[emu.sp];
            bump_pc = false;
            break;
        }
        default: {
            panic_opcode("unimplemented", opcode);
        }
    }
}

void emulate_0x8XYN_opcode_cycle(struct emulator& emu, const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0x8000);

    uint8_t& Vx = emu.V[ith_hex_digit<1>(opcode)];
    uint8_t& Vy = emu.V[ith_hex_digit<2>(opcode)];
    uint8_t& Vf = emu.V[0xF];

    switch (opcode & 0x000F) {
        case 0x0000: {  // set VX to the value currently in VY
            Vx = Vy;
            break;
        }
        case 0x0001: {  // set VX = VX | VY
            Vx |= Vy;
            break;
        }
        case 0x0002: {
            Vx &= Vy;
            break;
        }
        case 0x0003: {
            Vx ^= Vy;
            break;
        }
        case 0x0004: {  // add VY to VX and set carry bit if needed
            Vf = Vy > 0xFF - Vx ? 1 : 0;
            Vx += Vy;
            break;
        }
        case 0x0005: {
            Vf = Vx > Vy ? 1 : 0;
            Vx -= Vy;
            break;
        }
        case 0x0006: {
            Vf = (Vx & 1) > 0;
            Vx /= 2;
            break;
        }
        case 0x0007: {
            Vf = Vy > Vx ? 1 : 0;
            Vx = Vy - Vx;
            break;
        }
        case 0x000E: {
            Vf = (Vx & 128) > 0 ? 1 : 0;
            Vx *= 2;
            break;
        }
        default: {
            panic_opcode("unknown", opcode);
        }
    }
}

void emulate_0xFXNN_opcode_cycle(struct emulator& emu, const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0xF000);

    const uint16_t I = emu.idx;
    const uint16_t X = ith_hex_digit<1>(opcode);
    uint8_t& Vx = emu.V[X];

    switch (opcode & 0x00FF) {
        case 0x0007: {
            Vx = emu.delay_timer;
            break;
        }
        case 0x000A: {
            panic_opcode("unimplemented", opcode);
            break;
        }
        case 0x0015: {
            emu.delay_timer = Vx;
            break;
        }
        case 0x0018: {
            emu.sound_timer = Vx;
            break;
        }
        case 0x001E: {
            emu.idx += Vx;
            break;
        }
        case 0x0029: {
            emu.idx = 5 * ith_hex_digit<1>(opcode);
            assert(emu.idx < 80 && emu.idx >= 0);
            break;
        }
        case 0x0033: {
            panic_opcode("unimplemented", opcode);
            break;
        }
        case 0x0055: {
            for (uint16_t i = 0; i <= X; i++) {
                emu.memory[I + i] = emu.V[i];
            }
            break;
        }
        case 0x0065: {
            for (uint16_t i = 0; i <= X; i++) {
                emu.V[i] = emu.memory[I + i];
            }
            break;
        }
        default: {
            panic_opcode("unknown", opcode);
        }
    }
}

}  // namespace

namespace chip::core {

void emulate_cycle(struct emulator& emu)
{
    auto read_mem_byte_at = [&](uint16_t addr) -> uint8_t {
        assert(addr < emulator::memory_size_bytes);
        return emu.memory[addr];
    };

    // 1. fetch opcode
    const uint16_t opcode = (read_mem_byte_at(emu.pc) << 8) | read_mem_byte_at(emu.pc + 1);

    debug_print(
        "read [%lu] opcode 0x%04X "
        "at address 0x%08x\n",
        emu.cycles_emulated, opcode, emu.pc);

    bool bump_pc = true;

    // 2. execute opcode
    switch (opcode & 0xF000) {
        case 0x0000: {
            emulate_0x0NNN_opcode_cycle(emu, opcode, bump_pc);
            break;
        }
        case 0x1000: {  // 0x1NNN jump to address NNN
            emu.pc = opcode & 0x0FFF;
            bump_pc = false;
            break;
        }
        case 0x2000: {  // 0x2NNN: call subroutine at address NNN
            emu.stack_trace[emu.sp] = emu.pc;
            emu.sp++;
            emu.pc = opcode & 0x0FFF;
            bump_pc = false;
            break;
        }
        case 0x3000: {
            const uint8_t Vx = emu.V[ith_hex_digit<1>(opcode)];
            if (Vx == (opcode & 0x00FF)) {
                emu.pc += 2;
            }
            break;
        }
        case 0x4000: {
            panic_opcode("unimplemented", opcode);
            break;
        }
        case 0x5000: {
            panic_opcode("unimplemented", opcode);
            break;
        }
        case 0x6000: {  // 0x6XNN: Set VX register to NN
            emu.V[ith_hex_digit<1>(opcode)] = 0x00FF & opcode;
            break;
        }
        case 0x7000: {  // 0x7XNN: Add NN to VX (ignore carry)
            emu.V[ith_hex_digit<1>(opcode)] += (opcode & 0x00FF);
            break;
        }
        case 0x8000: {
            emulate_0x8XYN_opcode_cycle(emu, opcode);
            break;
        }
        case 0x9000: {
            panic_opcode("unimplemented", opcode);
            break;
        }
        case 0xA000: {
            emu.idx = opcode & 0x0FFF;
            emu.pc += 2;
            break;
        }
        case 0xB000: {
            panic_opcode("unimplemented", opcode);
            break;
        }
        case 0xC000: {
            panic_opcode("unimplemented", opcode);
            break;
        }
        case 0xD000: {  // 0xDXYN: Draw sprite to gfx buffer at coordinates (VX, VY).
            emu.draw_flag = true;

            const uint8_t Vx = emu.V[ith_hex_digit<1>(opcode)];
            const uint8_t Vy = emu.V[ith_hex_digit<2>(opcode)];
            const uint8_t N = emu.V[ith_hex_digit<3>(opcode)];

            uint8_t& Vf = emu.V[0xF];
            Vf = 0;

            for (auto i = 0; i < N; i++) {
                uint8_t sprite_bits = emu.memory[emu.idx + i];
                const uint8_t y = (Vy + i) % emulator::screen_height;

                uint8_t j = 0;
                while (sprite_bits != 0) {
                    const uint8_t x = (Vx + j) % emulator::screen_width;
                    bool& pixel_state = emu.gfx[y * emulator::screen_width + x];
                    const bool new_pixel_state = ((sprite_bits & 1) == 1) ^ pixel_state;
                    if (pixel_state && !new_pixel_state) Vf = 1;
                    pixel_state = new_pixel_state;

                    j++;
                    sprite_bits >>= 1;
                }
            }

            break;
        }
        case 0xE000: {
            panic_opcode("unimplemented", opcode);
            break;
        }
        case 0xF000: {
            emulate_0xFXNN_opcode_cycle(emu, opcode);
            break;
        }
        default: {
            panic_opcode("unknown", opcode);
        }
    }

    // 3. bump program counter if needed
    if (bump_pc) emu.pc += 2;

    // 4. update timers
    if (emu.delay_timer > 0) emu.delay_timer--;

    if (emu.sound_timer > 0) {
        emu.sound_timer--;
        if (emu.sound_timer == 0) printf("BEEP!\n");
    }

    emu.cycles_emulated++;

    emu.last_cycle = chip8::core::clock::now();
}

}  // namespace chip::core
