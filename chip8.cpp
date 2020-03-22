#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define DEBUG 1
#define debug_print(fmt, ...)                         \
    do {                                              \
        if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); \
    } while (0)

namespace chip8 {

struct emulator {
    static constexpr auto memory_size_bytes = 4096;
    static constexpr auto rom_memory_offset = 0x200;
    static constexpr auto allowed_rom_memory = memory_size_bytes - rom_memory_offset;
    static constexpr auto screen_width = 64;
    static constexpr auto screen_height = 32;
    static constexpr auto max_stack_depth = 16;
    static constexpr auto key_count = 16;
    static constexpr auto register_count = 16;

    uint8_t memory[memory_size_bytes] = {0};
    bool gfx[screen_width * screen_height] = {false};
    uint16_t stack_trace[max_stack_depth] = {0};
    uint8_t registers[register_count] = {0};
    uint8_t input[key_count] = {0};
    uint16_t idx = 0;     // index register
    uint16_t pc = 0x200;  // program counter
    uint16_t sp = 0;      // stack "pointer"
    uint8_t delay_timer = 0;
    uint8_t sound_timer = 0;
    bool draw_flag = false;
    uint64_t cycles_emulated = 0;
};

void load_fontset(struct emulator* emu)
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
        emu->memory[i] = chip8_fontset[i];
    }
}

inline void clear_screen(struct emulator* emu)
{
    for (auto i = 0; i < emulator::screen_width * emulator::screen_height; i++) {
        emu->gfx[i] = false;
    }
}

inline void panic_opcode(const char* description, const uint16_t opcode)
{
    printf("%s CHIP8 op-code encountered: 0x%04x\n", description, opcode);
    exit(1);
};

void emulate_0x0NNN_opcode(struct emulator* emu, const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0x0000);

    switch (opcode) {
        case 0x00E0:
            clear_screen(emu);
            break;
        case 0x00EE:
            panic_opcode("unimplemented", opcode);
            break;
        default:
            panic_opcode("unimplemented", opcode);
            break;
    }
}

void emulate_0x8XYN_opcode(struct emulator* emu, const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0x8000);

    switch (opcode & 0x000F) {
        case 0x0000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0001:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0002:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0003:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0004:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0005:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0006:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0007:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x000E:
            panic_opcode("unimplemented", opcode);
            break;
        default:
            panic_opcode("unknown", opcode);
    }
}

void emulate_0xFXNN_opcode(struct emulator* emu, const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0xF000);

    switch (opcode & 0x00FF) {
        case 0x0007:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x000A:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0015:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0018:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x001E:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0029:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0033:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0055:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x0065:
            panic_opcode("unimplemented", opcode);
            break;
        default:
            panic_opcode("unknown", opcode);
    }
}

void emulate_cycle(struct emulator* emu)
{
    auto read_byte_at = [&](uint16_t addr) -> uint8_t {
        assert(addr < emulator::memory_size_bytes);
        return emu->memory[addr];
    };

    // 1. fetch opcode
    const uint16_t opcode = read_byte_at(emu->pc) << 8 | read_byte_at(emu->pc + 1);
    debug_print("read opcode 0x%04x at address 0x%08x\n", opcode, emu->pc);

    // 2. execute opcode
    switch (opcode & 0xF000) {
        case 0x0000:
            emulate_0x0NNN_opcode(emu, opcode);
            break;
        case 0x1000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x2000:  // call subroutine
            emu->stack_trace[emu->sp] = emu->pc;
            emu->sp++;
            emu->pc = opcode & 0x0FFF;
            break;
        case 0x3000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x4000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x5000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x6000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x7000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0x8000:
            emulate_0x8XYN_opcode(emu, opcode);
            break;
        case 0x9000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0xA000:
            emu->idx = opcode & 0x0FFF;
            emu->pc += 2;
            break;
        case 0xB000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0xC000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0xD000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0xE000:
            panic_opcode("unimplemented", opcode);
            break;
        case 0xF000:
            emulate_0xFXNN_opcode(emu, opcode);
            break;
        default:
            panic_opcode("unknown", opcode);
    }

    // 3. update timers
    if (emu->delay_timer > 0) emu->delay_timer--;

    if (emu->sound_timer > 0) {
        emu->sound_timer--;
        if (emu->sound_timer == 0) printf("BEEP!\n");
    }

    emu->cycles_emulated++;
}

void load_game(struct emulator* emu, const char* rom_path)
{
    FILE* infile = fopen(rom_path, "rb");
    if (infile == nullptr) {
        printf("failed to load ROM from file: %s\n", rom_path);
        exit(EXIT_FAILURE);
    }

    fseek(infile, 0, SEEK_END);
    const auto file_size_bytes = ftell(infile);

    if (file_size_bytes < 0) {
        printf("error determining size of input ROM file: %s\n", rom_path);
        exit(EXIT_FAILURE);
    }

    rewind(infile);

    if (file_size_bytes > emulator::allowed_rom_memory) {
        printf("requested ROM file (%s) doesn't fit in CHIP8 memory!\n", rom_path);
        exit(EXIT_FAILURE);
    }

    auto buffer = (uint8_t*)malloc(sizeof(uint8_t) * file_size_bytes);

    if (buffer == nullptr) {
        printf("failed to allocate temporary heap space for loading rom!\n");
        exit(EXIT_FAILURE);
    }

    const auto read_result = fread(buffer, 1, file_size_bytes, infile);
    if (read_result != file_size_bytes) {
        printf("error reading input rom file: %s\n", rom_path);
        exit(EXIT_FAILURE);
    }

    for (auto i = 0; i < file_size_bytes; i++) {
        emu->memory[emulator::rom_memory_offset + i] = buffer[i];
    }

    printf("successfully loaded %lu bytes from %s into memory.\n", file_size_bytes,
           rom_path);

    free(buffer);
}

void draw_screen(struct emulator* emu) {}

void read_key_state(struct emulator* emu) {}

void setup_graphics(void) {}

void setup_input(void) {}

}  // namespace chip8

using namespace chip8;

int main(void)
{
    setup_graphics();
    setup_input();

    emulator emu;
    load_fontset(&emu);
    load_game(&emu, "./roms/pong.rom");

    while (true) {
        emulate_cycle(&emu);
        if (emu.draw_flag) draw_screen(&emu);
        read_key_state(&emu);
    }

    return EXIT_SUCCESS;
};

