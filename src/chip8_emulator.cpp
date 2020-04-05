#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "chip8_emulator.hpp"

#include "chip8_disassembler.hpp"
#include "chip8_prelude.hpp"

namespace chip8::emulator {

namespace {

constexpr bool CHIP8_DEBUG = true;

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

// see https://en.wikipedia.org/wiki/CHIP-8#Virtual_machine_description
uint8_t memory[memory_size_bytes];
bool gfx[pixel_count];
uint16_t stack_trace[max_stack_depth];
uint8_t V[register_count];
bool input[user_input_key_count];
timer delay_timer;
timer sound_timer;
int16_t register_awaiting_input;
uint16_t idx; // index register
uint16_t pc;  // program counter
uint16_t sp;  // stack "pointer"
uint64_t cycles_emulated;

uint8_t read_mem_byte_at(uint16_t addr)
{
    assert(addr < chip8::memory_size_bytes);
    return memory[addr];
};

void debug_print(const char* fmt, ...)
{
    if constexpr (CHIP8_DEBUG) {
        va_list argptr;
        va_start(argptr, fmt);
        vfprintf(stderr, fmt, argptr);
        va_end(argptr);
    }
}

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

    memcpy(memory, chip8_fontset, sizeof(uint8_t) * 80);
    memset(memory + 80, 0, sizeof(uint8_t) * (chip8::memory_size_bytes - 80));
    memset(gfx, false, sizeof(bool) * chip8::pixel_count);
    memset(stack_trace, 0, sizeof(uint16_t) * chip8::max_stack_depth);
    memset(V, 0, sizeof(uint8_t) * chip8::register_count);
    memset(input, false, sizeof(bool) * chip8::user_input_key_count);

    register_awaiting_input = -1;

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
            memset(gfx, false, sizeof(bool) * chip8::pixel_count);
            break;
        }

        case 0x00EE: { // RET: return from a subroutine
            assert(sp > 0);
            sp--;
            pc = stack_trace[sp];
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
            break;
        }
        case 0x0001: { // 0x8XY1
            Vx |= Vy;
            break;
        }
        case 0x0002: { // 0x8XY2
            Vx &= Vy;
            break;
        }
        case 0x0003: { // 0x8XY3
            Vx ^= Vy;
            break;
        }
        // @DEBUG
        case 0x0004: { // 0x8XY4: add VY to VX and set carry bit if needed
            Vf = Vy > 0xFF - Vx ? 1 : 0;
            Vx += Vy;
            break;
        }
        // @DEBUG
        case 0x0005: { // 0x8XY5: subtract VY from VX and set carry bit to "NOT borrow"
            Vf = Vx > Vy ? 1 : 0;
            Vx -= Vy;
            break;
        }
        case 0x0006: { // 0x8XY6: right bitshift VX by 1 (i.e. divide by 2) and set carry bit
                       // to one if VX is odd, otherwise zero
            Vf = Vx & 1;
            Vx /= 2;
            break;
        }
        case 0x0007: { // 0x8XY7: set VX = VY - VX and set carry flag to "NOT borrow"
            Vf = Vy > Vx ? 1 : 0;
            Vx = Vy - Vx;
            break;
        }
        case 0x000E: { // 0x8XYE: left bitshift VX by 1 (i.e. multiply by 2) and set carry bit
                       // to one if MSB of VX is set, otherwise zero
            Vf = (Vx >= 128) ? 1 : 0;
            Vx *= 2;
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
            break;
        }
        case 0x0018: { // 0xFX18
            sound_timer.write(Vx);
            break;
        }
        case 0x001E: { // 0xFX1E
            idx += Vx;
            break;
        }
        case 0x0029: { // 0xFX29: set index register to point to fontset location of VX
            idx = 5 * Vx;
            assert(idx < 80 && idx >= 0);
            break;
        }
        case 0x0033: { // 0xFX33: store BCD representation of VX at memory location of I
            memory[I] = (Vx / 100) % 10;
            memory[I + 1] = (Vx / 10) % 10;
            memory[I + 2] = Vx % 10;
            break;
        }
        case 0x0055: { // 0xFX55
            for (uint16_t i = 0; i <= X; i++) {
                memory[I + i] = V[i];
            }
            break;
        }
        case 0x0065: { // 0xFX65
            for (uint16_t i = 0; i <= X; i++) {
                V[i] = memory[I + i];
            }
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
    if (register_awaiting_input >= 0) return;

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
            break;
        }
        case 0x2000: { // 0x2NNN: call subroutine at address NNN
            stack_trace[sp] = pc;
            sp++;
            pc = NNN;
            bump_pc = false;
            break;
        }
        case 0x3000: { // 0x3XKK: skip next instruction if VX == kk
            if (Vx == KK) pc += 2;
            break;
        }
        case 0x4000: { // 0x4XKK: skip next instruction if VX != kk
            if (Vx != KK) pc += 2;
            break;
        }
        case 0x5000: { // 0x5XY0: skip next instruction if VX == VY
            if (Vx == Vy) pc += 2;
            break;
        }
        case 0x6000: { // 0x6XKK: Set VX register to KK
            Vx = KK;
            break;
        }
        case 0x7000: { // 0x7XKK: Add KK to VX (ignore carry)
            Vx += KK;
            break;
        }
        case 0x8000: {
            emulate_0x8XYN_opcode_cycle(opcode);
            break;
        }
        case 0x9000: { // 0x9XY0: skip next instruction if VX != VY
            if (Vx != Vy) pc += 2;
            break;
        }
        case 0xA000: { // 0xANNN: set index register to NNN
            idx = NNN;
            break;
        }
        case 0xB000: { // 0xBNNN: jump to location V0 + NNN
            pc = V[0] + NNN;
            bump_pc = false;
            break;
        }
        case 0xC000: { // 0xCXKK: generate a random number in the range [0,255], then AND it
                       // with KK and store in VX
            Vx = (rand() & 255) & KK; // (x % N) == (x & (n-1)) if n is a power of two
            break;
        }
        case 0xD000: { // 0xDXYN: draw sprite to gfx buffer at coordinates (VX, VY).
            const uint16_t N = ith_hex_digit<3>(opcode);

            uint8_t& Vf = V[0xF];
            Vf = 0;

            for (auto i = 0; i < N; i++) {
                uint8_t sprite_bits = memory[idx + i];
                const uint8_t y = (Vy + i) % chip8::display_grid_height;

                uint8_t j = 7;
                while (sprite_bits != 0) {
                    const uint8_t x = (Vx + j) % chip8::display_grid_width;
                    bool& pixel_state = gfx[y * chip8::display_grid_width + x];
                    const bool new_pixel_state = ((sprite_bits & 1) == 1) ^ pixel_state;
                    if (pixel_state && !new_pixel_state) Vf = 1;
                    pixel_state = new_pixel_state;

                    j--;
                    sprite_bits >>= 1;
                }
            }

            break;
        }
        case 0xE000: {
            assert(Vx < chip8::user_input_key_count);

            switch (opcode & 0x00FF) {
                case 0x009E: // 0xEX9E: skip next instruction if VXth key is pressed
                    assert(Vx < chip8::user_input_key_count);
                    if (input[Vx]) pc += 2;
                    break;
                case 0x00A1: // 0xEXA1: skip next instruction if VXth key is NOT pressed
                    assert(Vx < chip8::user_input_key_count);
                    if (!input[Vx]) pc += 2;
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

    if (file_size_bytes > chip8::allowed_rom_memory) {
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
        memory[chip8::rom_memory_offset + i] = buffer[i];
    }

    printf("successfully loaded %d bytes from %s into memory.\n\n", (int)file_size_bytes,
           rom_path);

    free(buffer);

    chip8::disassembler::init(memory + chip8::rom_memory_offset, &pc);
}

void terminate(void)
{
    reset();
    chip8::disassembler::terminate();
}

void update_user_input(uint8_t key_id, bool new_state)
{
    if (register_awaiting_input >= 0 && new_state && !input[key_id]) {
        const auto X = register_awaiting_input;
        V[X] = key_id;
        register_awaiting_input = -1;
        pc += 2;
        cycles_emulated++;
    }

    input[key_id] = new_state;
}

bool is_beeping(void) { return sound_timer.read() > 0; }

const bool* const get_screen_state(void) { return gfx; }

} // namespace chip8::emulator
