#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <optional>

#include "chip8_emulator.hpp"
#include "chip8_prelude.hpp"

namespace chip8::emulator {

namespace {

constexpr bool CHIP8_DEBUG = true;

// see https://en.wikipedia.org/wiki/CHIP-8#Virtual_machine_description
uint8_t memory[memory_size_bytes];

bool gfx[pixel_count];

uint16_t stack_trace[max_stack_depth];

uint8_t V[register_count];

bool input[user_input_key_count];

uint16_t idx; // index register
uint16_t pc;  // program counter
uint16_t sp;  // stack "pointer"

std::optional<uint16_t> register_awaiting_input;

uint64_t cycles_emulated;

void clear_screen(void) { memset(gfx, false, sizeof(bool) * emulator::pixel_count); }

class timer {
    static constexpr auto period = std::chrono::duration<double>(1.0 / 60.0);
    chip8::clock::time_point last_access_time = chip8::clock::now();
    uint8_t value = 0;

public:
    uint8_t read(void)
    {
        const auto orig_value = value;
        const auto current_time = chip8::clock::now();
        const auto nticks_double = floor((current_time - last_access_time) / period);
        const uint8_t nticks = nticks_double <= 255 ? (uint8_t)nticks_double : 0;

        if (nticks > 0) {
            last_access_time = current_time;
            value = value > nticks ? value - nticks : 0;
        }

        return value;
    }

    uint8_t write(uint8_t new_val)
    {
        last_access_time = chip8::clock::now();
        const auto old_val = value;
        value = new_val;
        return old_val;
    }
};

timer delay_timer;
timer sound_timer;

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
void record_instr(const char* format, Args... args)
{
    if constexpr (CHIP8_DEBUG) {
        size_t size = snprintf(nullptr, 0, format, args...) + 1; // Extra space for '\0'
        assert(size > 0);

        auto buf = (char*)malloc(size * sizeof(char));
        assert(buf != nullptr);
        snprintf(buf, size, format, args...);
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

void reset(void)
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
        memory[i] = chip8_fontset[i];
    }

    for (auto i = 80; i < emulator::memory_size_bytes; i++) {
        memory[i] = 0;
    }

    clear_screen();

    for (auto i = 0; i < emulator::max_stack_depth; i++) {
        stack_trace[i] = 0;
    }

    for (auto i = 0; i < emulator::register_count; i++) {
        V[i] = 0;
    }

    memset(input, false, sizeof(bool) * emulator::user_input_key_count);

    register_awaiting_input = {};

    idx = 0;
    pc = 0x200;
    sp = 0;

    delay_timer.write(0);
    sound_timer.write(0);

    cycles_emulated = 0;
}

void emulate_0x0NNN_opcode_cycle(const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0x0000);

    switch (opcode) {
        case 0x00E0: { // CLS: clear screen
            clear_screen();
            record_instr("CLS");
            break;
        }

        case 0x00EE: { // RET: return from a subroutine
            assert(sp > 0);
            sp--;
            pc = stack_trace[sp];
            record_instr("RET");
            break;
        }

        default: {
            panic_opcode("unknown", opcode);
        }
    }
}

void emulate_0x8XYN_opcode_cycle(const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0x8000);

    const uint16_t X = ith_hex_digit<1>(opcode);
    const uint16_t Y = ith_hex_digit<2>(opcode);
    uint8_t& Vx = V[X];
    uint8_t& Vy = V[Y];
    uint8_t& Vf = V[0xF];

    switch (opcode & 0x000F) {
        case 0x0000: { // 0x8XY0
            Vx = Vy;
            record_instr("LD V%01X, V%01X", X, Y);
            break;
        }
        case 0x0001: { // 0x8XY1
            Vx |= Vy;
            record_instr("OR V%01X, V%01X", X, Y);
            break;
        }
        case 0x0002: { // 0x8XY2
            Vx &= Vy;
            record_instr("AND V%01X, V%01X", X, Y);
            break;
        }
        case 0x0003: { // 0x8XY3
            Vx ^= Vy;
            record_instr("XOR V%01X, V%01X", X, Y);
            break;
        }
        // @DEBUG
        case 0x0004: { // 0x8XY4: add VY to VX and set carry bit if needed
            Vf = Vy > 0xFF - Vx ? 1 : 0;
            Vx += Vy;
            record_instr("ADD V%01X, V%01X", X, Y);
            break;
        }
        // @DEBUG
        case 0x0005: { // 0x8XY5: subtract VY from VX and set carry bit to "NOT borrow"
            Vf = Vx > Vy ? 1 : 0;
            Vx -= Vy;
            record_instr("SUB V%01X, V%01X", X, Y);
            break;
        }
        case 0x0006: { // 0x8XY6: right bitshift VX by 1 (i.e. divide by 2) and set carry bit
                       // to one if VX is odd, otherwise zero
            Vf = Vx & 1;
            Vx /= 2;
            record_instr("SHR V%01X", X);
            break;
        }
        case 0x0007: { // 0x8XY7: set VX = VY - VX and set carry flag to "NOT borrow"
            Vf = Vy > Vx ? 1 : 0;
            Vx = Vy - Vx;
            record_instr("SUBN V%01X, V%01X", X, Y);
            break;
        }
        case 0x000E: { // 0x8XYE: left bitshift VX by 1 (i.e. multiply by 2) and set carry bit
                       // to one if MSB of VX is set, otherwise zero
            Vf = (Vx >= 128) ? 1 : 0;
            Vx *= 2;
            record_instr("SHL V%01X", X);
            break;
        }
        default: {
            panic_opcode("unknown", opcode);
        }
    }
}

void emulate_0xFXNN_opcode_cycle(const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0xF000);

    const uint16_t I = idx;
    const uint16_t X = ith_hex_digit<1>(opcode);
    uint8_t& Vx = V[X];

    switch (opcode & 0x00FF) {
        case 0x0007: { // 0xFX07
            Vx = delay_timer.read();
            record_instr("LD V%01X, DT", X);
            break;
        }
        case 0x000A: { // 0xFX0A: Wait for a key press, and store the eventual key press in VX.
                       // This instruction doesn't actually finish until
                       // chip8::core::update_user_input is called after a new key press.
            register_awaiting_input = X;
            return;
        }
        case 0x0015: { // 0xFX15
            delay_timer.write(Vx);
            record_instr("LD DT, V%01X", X);
            break;
        }
        case 0x0018: { // 0xFX18
            sound_timer.write(Vx);
            record_instr("LD ST, V%01X", X);
            break;
        }
        case 0x001E: { // 0xFX1E
            idx += Vx;
            record_instr("ADD I, V%01X", X);
            break;
        }
        case 0x0029: { // 0xFX29: set index register to point to fontset location of VX
            idx = 5 * Vx;
            assert(idx < 80 && idx >= 0);
            record_instr("LD F, V%01X", X);
            break;
        }
        case 0x0033: { // 0xFX33: store BCD representation of VX at memory location of I
            memory[I] = (Vx / 100) % 10;
            memory[I + 1] = (Vx / 10) % 10;
            memory[I + 2] = Vx % 10;
            record_instr("LD B, V%01X", X);
            break;
        }
        case 0x0055: { // 0xFX55
            for (uint16_t i = 0; i <= X; i++) {
                memory[I + i] = V[i];
            }
            record_instr("LD [I], V%01X", X);
            break;
        }
        case 0x0065: { // 0xFX65
            for (uint16_t i = 0; i <= X; i++) {
                V[i] = memory[I + i];
            }
            record_instr("LD V%01X, [I]", X);
            break;
        }
        default: {
            panic_opcode("unknown", opcode);
        }
    }
}

} // namespace

void emulate_cycle(void)
{
    if (register_awaiting_input) return;

    auto read_mem_byte_at = [&](uint16_t addr) -> uint8_t {
        assert(addr < emulator::memory_size_bytes);
        return memory[addr];
    };

    const uint16_t opcode = (read_mem_byte_at(pc) << 8) | read_mem_byte_at(pc + 1);

    bool bump_pc = true;

    const uint16_t X = ith_hex_digit<1>(opcode);
    const uint16_t Y = ith_hex_digit<2>(opcode);
    const uint16_t NNN = opcode & 0x0FFF;
    const uint8_t KK = opcode & 0x00FF;
    uint8_t& Vx = V[X];
    uint8_t& Vy = V[Y];

    switch (opcode & 0xF000) {
        case 0x0000: {
            emulate_0x0NNN_opcode_cycle(opcode);
            break;
        }
        case 0x1000: { // 0x1NNN: jump to address NNN
            pc = NNN;
            bump_pc = false;
            record_instr("JP 0x%04X", NNN);
            break;
        }
        case 0x2000: { // 0x2NNN: call subroutine at address NNN
            stack_trace[sp] = pc;
            sp++;
            pc = NNN;
            bump_pc = false;
            record_instr("CALL 0x%03X", NNN);
            break;
        }
        case 0x3000: { // 0x3XKK: skip next instruction if VX == kk
            if (Vx == KK) pc += 2;
            record_instr("SE V%01x 0x%02X", X, KK);
            break;
        }
        case 0x4000: { // 0x4XKK: skip next instruction if VX != kk
            if (Vx != KK) pc += 2;
            record_instr("SNE V%01X, 0x%02X", X, KK);
            break;
        }
        case 0x5000: { // 0x5XY0: skip next instruction if VX == VY
            if (Vx == Vy) pc += 2;
            record_instr("SE V%01X, V%01X", X, Y);
            break;
        }
        case 0x6000: { // 0x6XKK: Set VX register to KK
            Vx = KK;
            record_instr("LD V%01X, 0x%02X", X, KK);
            break;
        }
        case 0x7000: { // 0x7XKK: Add KK to VX (ignore carry)
            Vx += KK;
            record_instr("ADD V%01X, 0x%02X", X, KK);
            break;
        }
        case 0x8000: {
            emulate_0x8XYN_opcode_cycle(opcode);
            break;
        }
        case 0x9000: { // 0x9XY0: skip next instruction if VX != VY
            if (Vx != Vy) pc += 2;
            record_instr("SNE V%01X, V%01X", X, Y);
            break;
        }
        case 0xA000: { // 0xANNN: set index register to NNN
            idx = NNN;
            record_instr("LD I, 0x%03X", NNN);
            break;
        }
        case 0xB000: { // 0xBNNN: jump to location V0 + NNN
            pc = V[0] + NNN;
            bump_pc = false;
            record_instr("JP V0, 0x%03X", NNN);
            break;
        }
        case 0xC000: { // 0xCXKK: generate a random number in the range [0,255], then AND it
                       // with KK and store in VX
            Vx = (rand() & 255) & KK; // (x % N) == (x & (n-1)) if n is a power of two
            record_instr("RND V%01X, 0x%02X", X, KK);
            break;
        }
        case 0xD000: { // 0xDXYN: draw sprite to gfx buffer at coordinates (VX, VY).
            const uint16_t N = ith_hex_digit<3>(opcode);

            uint8_t& Vf = V[0xF];
            Vf = 0;

            for (auto i = 0; i < N; i++) {
                uint8_t sprite_bits = memory[idx + i];
                const uint8_t y = (Vy + i) % emulator::display_grid_height;

                uint8_t j = 7;
                while (sprite_bits != 0) {
                    const uint8_t x = (Vx + j) % emulator::display_grid_width;
                    bool& pixel_state = gfx[y * emulator::display_grid_width + x];
                    const bool new_pixel_state = ((sprite_bits & 1) == 1) ^ pixel_state;
                    if (pixel_state && !new_pixel_state) Vf = 1;
                    pixel_state = new_pixel_state;

                    j--;
                    sprite_bits >>= 1;
                }
            }

            record_instr("DRW V%01X, V%01X, 0x%01X", X, Y, N);
            break;
        }
        case 0xE000: {
            assert(Vx < emulator::user_input_key_count);

            switch (opcode & 0x00FF) {
                case 0x009E: // 0xEX9E: skip next instruction if VXth key is pressed
                    assert(Vx < emulator::user_input_key_count);
                    if (input[Vx]) pc += 2;
                    record_instr("SKP V%01X", X);
                    break;
                case 0x00A1: // 0xEXA1: skip next instruction if VXth key is NOT pressed
                    assert(Vx < emulator::user_input_key_count);
                    if (!input[Vx]) pc += 2;
                    record_instr("SKNP V%01X", X);
                    break;
                default:
                    panic_opcode("unknown", opcode);
                    break;
            }

            break;
        }
        case 0xF000: {
            emulate_0xFXNN_opcode_cycle(opcode);
            break;
        }
        default: {
            panic_opcode("unknown", opcode);
        }
    }

    if (bump_pc) pc += 2;
    cycles_emulated++;
}

void init(const char* rom_path)
{
    FILE* rom_file = nullptr;
    errno_t read_err = 0;

#ifdef _MSC_VER
    read_err = fopen_s(&rom_file, rom_path, "rb");
#else
    rom_file = fopen(rom_path, "rb");
#endif

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

    if (file_size_bytes > emulator::allowed_rom_memory) {
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

    reset();

    for (uint64_t i = 0; i < file_size_bytes; i++) {
        memory[emulator::rom_memory_offset + i] = buffer[i];
    }

    printf("successfully loaded %d bytes from %s into memory.\n\n", (int)file_size_bytes,
           rom_path);

    free(buffer);
}

void terminate(void) { reset(); }

void update_user_input(uint8_t key_id, bool new_state)
{
    const bool old_state = input[key_id];

    if (register_awaiting_input && new_state && !old_state) {
        const auto X = *register_awaiting_input;
        V[X] = key_id;
        record_instr("LD V%01X, 0x%01X", X, key_id);
        register_awaiting_input = {};
        pc += 2;
        cycles_emulated++;
    }

    input[key_id] = new_state;
}

bool is_beeping(void) { return sound_timer.read() > 0; }

const bool* const get_screen_state(void) { return gfx; }

} // namespace chip8::emulator
