#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "chip8_core.hpp"

namespace chip8::core {

namespace {

constexpr bool CHIP8_DEBUG = true;
void debug_print(const char* fmt, ...)
{
    if constexpr (CHIP8_DEBUG) {
        va_list argptr;
        va_start(argptr, fmt);
        vfprintf(stderr, fmt, argptr);
        va_end(argptr);
    }
}

template <typename... Args>
void record_instr(struct emulator& emu, const std::string_view& format, Args... args)
{
    if constexpr (CHIP8_DEBUG) {
        size_t size = snprintf(nullptr, 0, format.data(), args...) + 1; // Extra space for '\0'
        assert(size > 0);

        auto buf = (char*)malloc(size * sizeof(char));
        assert(buf != nullptr);
        snprintf(buf, size, format.data(), args...);

        emu.instr_history.emplace_back(buf, buf + size - 1);

        if (emu.instr_history.size() > emu.history_size) {
            emu.instr_history.pop_front();
        }

        fprintf(stderr, "%s\n", emu.instr_history.back().c_str());

        free(buf);
    }
}

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

void reset(struct chip8::core::emulator& emu)
{
    static constexpr uint8_t chip8_fontset[80] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    for (auto i = 0; i < 80; i++) {
        emu.memory[i] = chip8_fontset[i];
    }

    for (auto i = 80; i < chip8::core::emulator::memory_size_bytes; i++) {
        emu.memory[i] = 0;
    }

    for (auto i = 0; i < chip8::core::emulator::pixel_count; i++) {
        emu.gfx[i] = false;
    }

    for (auto i = 0; i < emulator::max_stack_depth; i++) {
        emu.stack_trace[i] = 0;
    }

    for (auto i = 0; i < emulator::register_count; i++) {
        emu.V[i] = 0;
    }

    for (auto i = 0; i < emulator::user_input_key_count; i++) {
        emu.input[i] = false;
    }

    emu.idx = 0;
    emu.pc = 0x200;
    emu.sp = 0;

    emu.delay_timer = 0;
    emu.sound_timer = 0;

    emu.register_awaiting_input = {};

    emu.draw_flag = false;

    emu.last_cycle = chip8::clock::now();
    emu.cycles_emulated = 0;

    if constexpr (CHIP8_DEBUG) {
        emu.history_size = 2048;
    }
    else {
        emu.history_size = 0;
    }
}

void emulate_0x0NNN_opcode_cycle(struct emulator& emu, const uint16_t opcode, bool& bump_pc)
{
    assert((opcode & 0xF000) == 0x0000);

    switch (opcode) {
        case 0x00E0: { // CLS: clear screen
            for (auto i = 0; i < chip8::core::emulator::pixel_count; i++) {
                emu.gfx[i] = false;
            }
            emu.draw_flag = true;
            record_instr(emu, "CLS");
            break;
        }

        case 0x00EE: { // RET: return from a subroutine
            assert(emu.sp > 0);
            emu.sp--;
            emu.pc = emu.stack_trace[emu.sp];
            bump_pc = false;
            record_instr(emu, "RET");
            break;
        }

        default: {
            panic_opcode("unknown", opcode);
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
        case 0x0000: { // set VX to the value currently in VY
            Vx = Vy;
            break;
        }
        case 0x0001: { // set VX = VX | VY
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
        case 0x0004: { // add VY to VX and set carry bit if needed
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
            emu.register_awaiting_input = X;
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
            emu.memory[I + 0] = (Vx / 100) % 10;
            emu.memory[I + 1] = (Vx / 10) % 10;
            emu.memory[I + 2] = Vx % 10;
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

} // namespace

void emulate_cycle(struct emulator& emu)
{
    if (emu.register_awaiting_input) return;

    auto read_mem_byte_at = [&](uint16_t addr) -> uint8_t {
        assert(addr < emulator::memory_size_bytes);
        return emu.memory[addr];
    };

    // 1. fetch opcode
    const uint16_t opcode = (read_mem_byte_at(emu.pc) << 8) | read_mem_byte_at(emu.pc + 1);

    bool bump_pc = true;

    const uint16_t X = ith_hex_digit<1>(opcode);
    const uint16_t Y = ith_hex_digit<2>(opcode);
    const uint16_t NNN = opcode & 0x0FFF;
    const uint8_t KK = opcode & 0x00FF;
    uint8_t& Vx = emu.V[X];
    uint8_t& Vy = emu.V[Y];

    // 2. execute opcode
    switch (opcode & 0xF000) {
        case 0x0000: {
            emulate_0x0NNN_opcode_cycle(emu, opcode, bump_pc);
            break;
        }
        case 0x1000: { // 0x1NNN jump to address NNN
            emu.pc = NNN;
            bump_pc = false;
            record_instr(emu, "JP 0x%04X", NNN);
            break;
        }
        case 0x2000: { // 0x2NNN: call subroutine at address NNN
            emu.stack_trace[emu.sp] = emu.pc;
            emu.sp++;
            emu.pc = NNN;
            bump_pc = false;
            record_instr(emu, "CALL 0x%04X", NNN);
            break;
        }
        case 0x3000: {
            if (Vx == KK) emu.pc += 2;
            // record_instr(emu, "CALL 0x%04X", NNN);
            break;
        }
        case 0x4000: {
            if (Vx != KK) emu.pc += 2;
            // record_instr(emu, "SE 0x%04X, 0x%04X", X, KK);
            break;
        }
        case 0x5000: { // 0x5XY0
            if (Vx == Vy) emu.pc += 2;
            break;
        }
        case 0x6000: { // 0x6XNN: Set VX register to NN
            Vx = KK;
            break;
        }
        case 0x7000: { // 0x7XNN: Add NN to VX (ignore carry)
            Vx += KK;
            break;
        }
        case 0x8000: {
            emulate_0x8XYN_opcode_cycle(emu, opcode);
            break;
        }
        case 0x9000: {
            if (Vx != Vy) emu.pc += 2;
            break;
        }
        case 0xA000: {
            emu.idx = NNN;
            emu.pc += 2;
            break;
        }
        case 0xB000: {
            emu.pc = emu.V[0] + NNN;
            bump_pc = false;
            break;
        }
        case 0xC000: {
            Vx = rand() & 255;
            break;
        }
        case 0xD000: { // 0xDXYN: Draw sprite to gfx buffer at coordinates (VX, VY).
            const uint16_t N = ith_hex_digit<3>(opcode);

            uint8_t& Vf = emu.V[0xF];
            Vf = 0;

            for (auto i = 0; i < N; i++) {
                uint8_t sprite_bits = emu.memory[emu.idx + i];
                const uint8_t y = (Vy + i) % emulator::display_grid_height;

                uint8_t j = 0;
                while (sprite_bits != 0) {
                    const uint8_t x = (Vx + j) % emulator::display_grid_width;
                    bool& pixel_state = emu.gfx[y * emulator::display_grid_width + x];
                    const bool new_pixel_state = ((sprite_bits & 1) == 1) ^ pixel_state;
                    if (pixel_state && !new_pixel_state) Vf = 1;
                    if (pixel_state != new_pixel_state) emu.draw_flag = true;
                    pixel_state = new_pixel_state;

                    j++;
                    sprite_bits >>= 1;
                }
            }

            break;
        }
        case 0xE000: {
            assert(Vx < chip8::core::emulator::user_input_key_count);

            switch (opcode & 0x00FF) {
                case 0x009E:
                    if (emu.input[Vx]) emu.pc += 2;
                    break;
                case 0x00A1:
                    if (!emu.input[Vx]) emu.pc += 2;
                    break;
                default:
                    panic_opcode("unknown", opcode);
                    break;
            }

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
    if (emu.sound_timer > 0) emu.sound_timer--;
    emu.cycles_emulated++;

    emu.last_cycle = chip8::clock::now();
}

struct emulator create_emulator(const char* rom_path)
{
    FILE* rom_file = nullptr;
    errno_t read_err = fopen_s(&rom_file, rom_path, "rb");

    if (rom_file == nullptr || read_err != 0) {
        printf("failed to load ROM from file: %s\n", rom_path);
        exit(EXIT_FAILURE);
    }

    fseek(rom_file, 0, SEEK_END);
    const auto ftell_ret = ftell(rom_file);
    rewind(rom_file);

    if (ftell_ret < 0) {
        printf("error determining size of input ROM file: %s\n", rom_path);
        exit(EXIT_FAILURE);
    }

    const auto file_size_bytes = static_cast<uint64_t>(ftell_ret);

    if (file_size_bytes > chip8::core::emulator::allowed_rom_memory) {
        printf("requested ROM file (%s) doesn't fit in CHIP8 memory!\n", rom_path);
        exit(EXIT_FAILURE);
    }

    auto buffer = (uint8_t*)malloc(sizeof(uint8_t) * file_size_bytes);

    if (buffer == nullptr) {
        printf("failed to allocate temporary heap space for loading rom!\n");
        exit(EXIT_FAILURE);
    }

    const uint64_t file_bytes_read = fread(buffer, 1, file_size_bytes, rom_file);
    if (file_bytes_read != file_size_bytes) {
        printf("error reading input rom file: %s\n", rom_path);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    emulator emu;
    reset(emu);

    for (uint64_t i = 0; i < file_size_bytes; i++) {
        emu.memory[emulator::rom_memory_offset + i] = buffer[i];
    }

    printf("successfully loaded %d bytes from %s into memory.\n\n", (int)file_size_bytes,
           rom_path);

    free(buffer);

    return emu;
}

void update_user_input(struct emulator& emu,
                       const bool new_input[emulator::user_input_key_count])
{
    if (emu.register_awaiting_input) {
        const auto& old_input = emu.input;
        for (uint8_t ikey = 0; ikey < emulator::user_input_key_count; ikey++) {
            if (!old_input[ikey] && new_input[ikey]) { // => key was pressed
                emu.V[*(emu.register_awaiting_input)] = ikey;
                emu.register_awaiting_input = {};
                break;
            }
        }
    }

    for (uint8_t ikey = 0; ikey < emulator::user_input_key_count; ikey++) {
        emu.input[ikey] = new_input[ikey];
    }
}

} // namespace chip8::core