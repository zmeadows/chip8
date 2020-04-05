#include "chip8_disassembler.hpp"

#include <cassert>
#include <cstring>

#include "chip8_prelude.hpp"

namespace chip8::disassembler {

namespace {

static constexpr auto line_length = 32;
static constexpr auto buffer_size = line_length * chip8::maximum_rom_instruction_count;

char buffer[buffer_size];
size_t instructions_disassembled;
size_t mru_win_start;
static constexpr auto mru_win_length = 32;

const uint16_t* pc_ptr = nullptr;

void reset(void)
{
    memset(buffer, '\0', buffer_size);
    instructions_disassembled = 0;
}

template <typename... Args>
void append(const char* format, Args... args)
{
    size_t size = snprintf(nullptr, 0, format, args...) + 1; // Extra space for '\0'
    assert(size > 0 && size < line_length);
    char* line_start = buffer + line_length * instructions_disassembled;
    snprintf(line_start, size, format, args...);
    instructions_disassembled++;
}

void disassemble_opcode(uint16_t opcode)
{
    const uint16_t X = chip8::ith_hex_digit<1>(opcode);
    const uint16_t Y = chip8::ith_hex_digit<2>(opcode);
    const uint16_t NNN = opcode & 0x0FFF;
    const uint8_t KK = opcode & 0x00FF;

    switch (opcode & 0xF000) {
        case 0x0000: {
            switch (opcode) {
                case 0x00E0: { // CLS: clear screen
                    append("CLS");
                    break;
                }
                case 0x00EE: { // RET: return from a subroutine
                    append("RET");
                    break;
                }
                default: {
                    panic_opcode("unknown", opcode);
                }
            }
            break;
        }
        case 0x1000: { // 0x1NNN
            append("JP 0x%04X", NNN);
            break;
        }
        case 0x2000: { // 0x2NNN
            append("CALL 0x%03X", NNN);
            break;
        }
        case 0x3000: { // 0x3XKK
            append("SE V%01x 0x%02X", X, KK);
            break;
        }
        case 0x4000: { // 0x4XKK
            append("SNE V%01X, 0x%02X", X, KK);
            break;
        }
        case 0x5000: { // 0x5XY0
            append("SE V%01X, V%01X", X, Y);
            break;
        }
        case 0x6000: { // 0x6XKK
            append("LD V%01X, 0x%02X", X, KK);
            break;
        }
        case 0x7000: { // 0x7XKK
            append("ADD V%01X, 0x%02X", X, KK);
            break;
        }
        case 0x8000: {
            switch (opcode & 0x000F) {
                case 0x0000: { // 0x8XY0
                    append("LD V%01X, V%01X", X, Y);
                    break;
                }
                case 0x0001: { // 0x8XY1
                    append("OR V%01X, V%01X", X, Y);
                    break;
                }
                case 0x0002: { // 0x8XY2
                    append("AND V%01X, V%01X", X, Y);
                    break;
                }
                case 0x0003: { // 0x8XY3
                    append("XOR V%01X, V%01X", X, Y);
                    break;
                }
                case 0x0004: { // 0x8XY4
                    append("ADD V%01X, V%01X", X, Y);
                    break;
                }
                case 0x0005: { // 0x8XY5
                    append("SUB V%01X, V%01X", X, Y);
                    break;
                }
                case 0x0006: { // 0x8XY6
                    append("SHR V%01X", X);
                    break;
                }
                case 0x0007: { // 0x8XY7
                    append("SUBN V%01X, V%01X", X, Y);
                    break;
                }
                case 0x000E: { // 0x8XYE
                    append("SHL V%01X", X);
                    break;
                }
                default: {
                    panic_opcode("unknown", opcode);
                }
            }
            break;
        }
        case 0x9000: { // 0x9XY0
            append("SNE V%01X, V%01X", X, Y);
            break;
        }
        case 0xA000: { // 0xANNN
            append("LD I, 0x%03X", NNN);
            break;
        }
        case 0xB000: { // 0xBNNN
            append("JP V0, 0x%03X", NNN);
            break;
        }
        case 0xC000: { // 0xCXKK
            append("RND V%01X, 0x%02X", X, KK);
            break;
        }
        case 0xD000: { // 0xDXYN
            const uint16_t N = ith_hex_digit<3>(opcode);
            append("DRW V%01X, V%01X, 0x%01X", X, Y, N);
            break;
        }
        case 0xE000: {
            switch (opcode & 0x00FF) {
                case 0x009E: // 0xEX9E
                    append("SKP V%01X", X);
                    break;
                case 0x00A1: // 0xEXA1
                    append("SKNP V%01X", X);
                    break;
                default:
                    panic_opcode("unknown", opcode);
                    break;
            }

            break;
        }
        case 0xF000: {

            switch (opcode & 0x00FF) {
                case 0x0007: { // 0xFX07
                    append("LD V%01X, DT", X);
                    break;
                }
                case 0x000A: { // 0xFX0A
                    append("LD V%01X, K", X);
                    return;
                }
                case 0x0015: { // 0xFX15
                    append("LD DT, V%01X", X);
                    break;
                }
                case 0x0018: { // 0xFX18
                    append("LD ST, V%01X", X);
                    break;
                }
                case 0x001E: { // 0xFX1E
                    append("ADD I, V%01X", X);
                    break;
                }
                case 0x0029: { // 0xFX29: set index register to point to fontset location of VX
                    append("LD F, V%01X", X);
                    break;
                }
                case 0x0033: { // 0xFX33
                    append("LD B, V%01X", X);
                    break;
                }
                case 0x0055: { // 0xFX55
                    append("LD [I], V%01X", X);
                    break;
                }
                case 0x0065: { // 0xFX65
                    append("LD V%01X, [I]", X);
                    break;
                }
                default: {
                    panic_opcode("unknown", opcode);
                }
            }
            break;
        }
        default: {
            panic_opcode("unknown", opcode);
        }
    }
}

} // namespace

void init(const uint8_t* const rom_memory, const uint16_t* program_counter)
{
    reset();

    for (auto i = 0; i < chip8::maximum_rom_instruction_count; i++) {
        const uint16_t opcode = (rom_memory[2 * i] << 8) | rom_memory[2 * i + 1];
        disassemble_opcode(opcode);
    }

    pc_ptr = program_counter;
}

void terminate(void) { reset(); }

template <typename Callable>
void iterate_mru_window(Callable&& f)
{
    const uint16_t pc = *pc_ptr;
    // if the PC has moved far enough, re-position the window of instructions we display
    if (pc < mru_win_start || pc >= mru_win_start + mru_win_length) {
        mru_win_start = *pc_ptr;
    }

    for (uint16_t imem = mru_win_start; imem < mru_win_start + mru_win_length; imem++) {
        f(buffer[imem], imem == pc);
    }
}

} // namespace chip8::disassembler