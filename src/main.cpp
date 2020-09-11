/****************************** LICENSE *************************************
Copyright (c) 2020 Zachary A. Meadows

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/

/*
This is a cross-platform graphical emulator for the CHIP-8 Virtual Machine.
See https://en.wikipedia.org/wiki/CHIP-8
or http://devernay.free.fr/hacks/chip8/C8TECH10.HTM for more details.
*/

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <optional>

#define MA_NO_DECODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h" // https://miniaud.io/

#include <GL/glew.h>    // http://glew.sourceforge.net/
#include <GLFW/glfw3.h> // https://www.glfw.org/

namespace fs = std::filesystem;
using namespace std::chrono;

#ifndef CHIP8_ASSETS_DIR
#define CHIP8_ASSETS_DIR "./"
#endif

#ifndef CHIP8_VERSION_MAJOR
#define CHIP8_VERSION_MAJOR 0
#endif

#ifndef CHIP8_VERSION_MINOR
#define CHIP8_VERSION_MINOR 0
#endif

namespace {

// Defer macro by Arthur O'Dwyer:
// https://quuxplusone.github.io/blog/2018/08/11/the-auto-macro/
template <class L>
class AtScopeExit {
    L& m_lambda;

public:
    AtScopeExit(L& action) : m_lambda(action) {}
    ~AtScopeExit() { m_lambda(); }
};

#define TOKEN_PASTEx(x, y) x##y
#define TOKEN_PASTE(x, y) TOKEN_PASTEx(x, y)

#define Auto_INTERNAL1(lname, aname, ...)                                                     \
    auto lname = [&]() { __VA_ARGS__; };                                                      \
    AtScopeExit<decltype(lname)> aname(lname);

#define Auto_INTERNAL2(ctr, ...)                                                              \
    Auto_INTERNAL1(TOKEN_PASTE(Auto_func_, ctr), TOKEN_PASTE(Auto_instance_, ctr), __VA_ARGS__)

#define Defer(...) Auto_INTERNAL2(__COUNTER__, __VA_ARGS__)

} // namespace

namespace chip8 {

constexpr auto MEMORY_SIZE_BYTES = 4096;
constexpr auto ROM_MEMORY_OFFSET = 0x200;
constexpr auto ALLOWED_ROM_MEMORY = MEMORY_SIZE_BYTES - ROM_MEMORY_OFFSET;
constexpr auto MAXIMUM_ROM_INSTRUCTION_COUNT = ALLOWED_ROM_MEMORY / 2;
constexpr auto DISPLAY_GRID_WIDTH = 64;
constexpr auto DISPLAY_GRID_HEIGHT = 32;
constexpr auto PIXEL_COUNT = DISPLAY_GRID_WIDTH * DISPLAY_GRID_HEIGHT;
constexpr auto MAX_STACK_DEPTH = 16;
constexpr auto USER_INPUT_KEY_COUNT = 16;
constexpr auto REGISTER_COUNT = 16;
constexpr auto INSTRUCTIONS_PER_FRAME = 5;

// typedef for the highest resolution steady clock
using clock =
    std::conditional<high_resolution_clock::is_steady, high_resolution_clock,
                     std::conditional<system_clock::period::den <= steady_clock::period::den,
                                      system_clock, steady_clock>::type>::type;

struct AudioContext {
    static constexpr auto DEVICE_FORMAT = ma_format_f32;
    static constexpr auto DEVICE_CHANNELS = 2;
    static constexpr auto DEVICE_SAMPLE_RATE = 48000;
    static constexpr auto SINE_WAVE_FREQUENCY = 250;

    ma_device device;
    ma_device_config device_config;
    ma_waveform sine_wave;
    ma_waveform_config sine_wave_config;
    bool init_success = false;

    static std::unique_ptr<AudioContext> create();
    ~AudioContext()
    {
        if (init_success) ma_device_uninit(&this->device);
    }

private:
    AudioContext() = default;
};

std::unique_ptr<AudioContext> AudioContext::create()
{
    auto ctx = std::unique_ptr<AudioContext>(new AudioContext());

    ctx->sine_wave_config =
        ma_waveform_config_init(DEVICE_FORMAT, DEVICE_CHANNELS, DEVICE_SAMPLE_RATE,
                                ma_waveform_type_sine, 0.2, SINE_WAVE_FREQUENCY);

    ma_waveform_init(&ctx->sine_wave_config, &ctx->sine_wave);

    ctx->device_config = ma_device_config_init(ma_device_type_playback);
    ctx->device_config.playback.format = DEVICE_FORMAT;
    ctx->device_config.playback.channels = DEVICE_CHANNELS;
    ctx->device_config.sampleRate = DEVICE_SAMPLE_RATE;
    ctx->device_config.pUserData = &ctx->sine_wave;

    ctx->device_config.dataCallback = [](ma_device* pDevice, void* pOutput,
                                         const void* /* pInput */, ma_uint32 frameCount) {
        ma_waveform* pSineWave;

        MA_ASSERT(pDevice->playback.channels == AudioContext::DEVICE_CHANNELS);

        pSineWave = (ma_waveform*)pDevice->pUserData;
        MA_ASSERT(pSineWave != NULL);

        ma_waveform_read_pcm_frames(pSineWave, pOutput, frameCount);
    };

    if (ma_device_init(NULL, &ctx->device_config, &ctx->device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to open audio playback device.\n");
        return nullptr;
    }

    ctx->init_success = true;

    fprintf(stderr, "Audio Device Name: %s\n", ctx->device.playback.name);

    return ctx;
}

void start_beep(AudioContext* audio)
{
    if (ma_device_start(&audio->device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&audio->device);
        return;
    }
}

void stop_beep(AudioContext* audio)
{
    if (ma_device_stop(&audio->device) != MA_SUCCESS) {
        printf("Failed to stop playback device.\n");
        ma_device_uninit(&audio->device);
        return;
    }
}

// Represents an 8-bit number that, once set, decrements automatically at 60 Hz
class Timer {
    static constexpr auto PERIOD = std::chrono::duration<double>(1.0 / 60.0);
    chip8::clock::time_point last_access_time = chip8::clock::now();
    uint8_t value = 0;

public:
    void write(uint8_t new_val);
    uint8_t read(void);
};

void Timer::write(uint8_t new_val)
{
    last_access_time = chip8::clock::now();
    value = new_val;
}

uint8_t Timer::read(void)
{
    const uint8_t orig_value = value;
    const chip8::clock::time_point current_time = chip8::clock::now();
    const double nticks_d = floor((current_time - last_access_time) / PERIOD);

    if (nticks_d > 255.0) {
        value = 0;
    }
    else if (nticks_d > 0.f) {
        const uint8_t nticks = (uint8_t)nticks_d;
        last_access_time = current_time;
        value = value > nticks ? value - nticks : 0;
    }

    return value;
}

struct Emulator {
    uint8_t memory[MEMORY_SIZE_BYTES];
    bool gfx[PIXEL_COUNT];
    uint16_t stack_trace[MAX_STACK_DEPTH];
    uint8_t V[REGISTER_COUNT];
    bool input[USER_INPUT_KEY_COUNT];
    Timer delay_timer;
    Timer sound_timer;
    int16_t register_awaiting_input;
    uint16_t idx; // index register
    uint16_t pc;  // program counter
    uint16_t sp;  // stack "pointer"
    uint64_t cycles_emulated;

    static std::unique_ptr<Emulator> create(const char* rom_path);

private:
    Emulator() = default;
};

inline void panic_opcode(const char* description, const uint16_t opcode)
{
    printf("%s CHIP8 op-code encountered: 0x%04X\n", description, opcode);
    exit(1);
};

void reset_emulator(Emulator* emu)
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

    memset(emu->memory, 0, sizeof(uint8_t) * chip8::MEMORY_SIZE_BYTES);
    memcpy(emu->memory, chip8_fontset, sizeof(uint8_t) * 80);
    memset(emu->gfx, false, sizeof(bool) * chip8::PIXEL_COUNT);
    memset(emu->stack_trace, 0, sizeof(uint16_t) * chip8::MAX_STACK_DEPTH);
    memset(emu->V, 0, sizeof(uint8_t) * chip8::REGISTER_COUNT);
    memset(emu->input, false, sizeof(bool) * chip8::USER_INPUT_KEY_COUNT);

    emu->register_awaiting_input = -1;

    emu->idx = 0;
    emu->pc = chip8::ROM_MEMORY_OFFSET;
    emu->sp = 0;

    emu->delay_timer.write(0);
    emu->sound_timer.write(0);

    emu->cycles_emulated = 0;
}

std::unique_ptr<Emulator> Emulator::create(const char* rom_path)
{
    auto emu = std::unique_ptr<Emulator>(new Emulator());

    FILE* rom_file = nullptr;
    errno_t read_err = 0;

#ifdef _MSC_VER
    read_err = fopen_s(&rom_file, rom_path, "rb");
#else
    rom_file = fopen(rom_path, "rb");
#endif

    if (rom_file == nullptr || read_err != 0) {
        fprintf(stderr, "failed to load ROM from file: %s\n", rom_path);
        return nullptr;
    }
    Defer(fclose(rom_file));

    fseek(rom_file, 0, SEEK_END);
    const auto ftell_ret = ftell(rom_file);
    rewind(rom_file);
    if (ftell_ret < 0) {
        fprintf(stderr, "error determining size of input ROM file: %s\n", rom_path);
        return nullptr;
    }

    const auto file_size_bytes = static_cast<uint64_t>(ftell_ret);
    if (file_size_bytes > chip8::ALLOWED_ROM_MEMORY) {
        fprintf(stderr, "Requested ROM file (%s) doesn't fit in CHIP8 memory!\n", rom_path);
        return nullptr;
    }

    auto buffer = (uint8_t*)malloc(sizeof(uint8_t) * file_size_bytes);
    if (buffer == nullptr) {
        fprintf(stderr, "Failed to allocate temporary heap space for loading rom!\n");
        return nullptr;
    }
    Defer(free(buffer));

    const uint64_t file_bytes_read = fread(buffer, 1, file_size_bytes, rom_file);
    if (file_bytes_read != file_size_bytes) {
        fprintf(stderr, "Error reading input rom file: %s\n", rom_path);
        return nullptr;
    }

    reset_emulator(emu.get());

    for (uint64_t i = 0; i < file_size_bytes; i++) {
        emu->memory[chip8::ROM_MEMORY_OFFSET + i] = buffer[i];
    }

    fprintf(stderr, "Successfully loaded %d bytes from %s into memory.\n\n",
            (int)file_size_bytes, rom_path);

    return emu;
}

// Extract individual digits from the hex representation.
// ith_hex_digit<0>(0xABCD) = A
// ith_hex_digit<1>(0xABCD) = B
// ith_hex_digit<2>(0xABCD) = C
// ith_hex_digit<3>(0xABCD) = D
template <uint16_t index>
constexpr uint16_t ith_hex_digit(uint16_t opcode)
{
    static_assert(index >= 0 && index <= 3);
    constexpr uint16_t offset = 12 - index * 4;
    constexpr uint16_t mask = 0x000F << offset;
    return (mask & opcode) >> offset;
}

void emulate_0x0NNN_opcode_cycle(Emulator* emu, const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0x0000);

    switch (opcode) {
        case 0x00E0: { // CLS: clear screen
            memset(emu->gfx, false, sizeof(bool) * chip8::PIXEL_COUNT);
            break;
        }

        case 0x00EE: { // RET: return from a subroutine
            assert(emu->sp > 0);
            emu->sp--;
            emu->pc = emu->stack_trace[emu->sp];
            break;
        }

        default: {
            panic_opcode("unknown", opcode);
        }
    }
}

void emulate_0x8XYN_opcode_cycle(Emulator* emu, const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0x8000);

    const uint16_t X = ith_hex_digit<1>(opcode);
    const uint16_t Y = ith_hex_digit<2>(opcode);
    uint8_t& Vx = emu->V[X];
    uint8_t& Vy = emu->V[Y];
    uint8_t& Vf = emu->V[0xF];

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

void emulate_0xFXNN_opcode_cycle(Emulator* emu, const uint16_t opcode)
{
    assert((opcode & 0xF000) == 0xF000);

    const uint16_t I = emu->idx;
    const uint16_t X = ith_hex_digit<1>(opcode);
    uint8_t& Vx = emu->V[X];

    switch (opcode & 0x00FF) {
        case 0x0007: { // 0xFX07
            Vx = emu->delay_timer.read();
            break;
        }
        case 0x000A: { // 0xFX0A: Wait for a key press, and store the eventual key press in VX.
                       // This instruction doesn't actually finish until
                       // chip8::core::update_user_input is called after a new key press.
            emu->register_awaiting_input = X;
            return;
        }
        case 0x0015: { // 0xFX15
            emu->delay_timer.write(Vx);
            break;
        }
        case 0x0018: { // 0xFX18
            emu->sound_timer.write(Vx);
            break;
        }
        case 0x001E: { // 0xFX1E
            emu->idx += Vx;
            break;
        }
        case 0x0029: { // 0xFX29: set index register to point to fontset location of VX
            emu->idx = 5 * Vx;
            assert(emu->idx < 80 && emu->idx >= 0);
            break;
        }
        case 0x0033: { // 0xFX33: store BCD representation of VX at memory location of I
            emu->memory[I] = (Vx / 100) % 10;
            emu->memory[I + 1] = (Vx / 10) % 10;
            emu->memory[I + 2] = Vx % 10;
            break;
        }
        case 0x0055: { // 0xFX55
            for (uint16_t i = 0; i <= X; i++) {
                emu->memory[I + i] = emu->V[i];
            }
            break;
        }
        case 0x0065: { // 0xFX65
            for (uint16_t i = 0; i <= X; i++) {
                emu->V[i] = emu->memory[I + i];
            }
            break;
        }
        default: {
            panic_opcode("unknown", opcode);
        }
    }
}

void emulate_cycle(Emulator* emu)
{
    if (emu->register_awaiting_input >= 0) return;

    auto read_mem_byte_at = [emu](uint16_t addr) -> uint16_t {
        assert(addr < chip8::MEMORY_SIZE_BYTES);
        return emu->memory[addr];
    };

    const uint16_t opcode = (read_mem_byte_at(emu->pc) << 8) | read_mem_byte_at(emu->pc + 1);

    bool bump_pc = true;

    const uint16_t X = ith_hex_digit<1>(opcode);
    const uint16_t Y = ith_hex_digit<2>(opcode);
    const uint16_t NNN = opcode & 0x0FFF;
    const uint8_t KK = opcode & 0x00FF;
    uint8_t& Vx = emu->V[X];
    uint8_t& Vy = emu->V[Y];

    switch (opcode & 0xF000) {
        case 0x0000: {
            emulate_0x0NNN_opcode_cycle(emu, opcode);
            break;
        }
        case 0x1000: { // 0x1NNN: jump to address NNN
            emu->pc = NNN;
            bump_pc = false;
            break;
        }
        case 0x2000: { // 0x2NNN: call subroutine at address NNN
            emu->stack_trace[emu->sp] = emu->pc;
            emu->sp++;
            emu->pc = NNN;
            bump_pc = false;
            break;
        }
        case 0x3000: { // 0x3XKK: skip next instruction if VX == kk
            if (Vx == KK) emu->pc += 2;
            break;
        }
        case 0x4000: { // 0x4XKK: skip next instruction if VX != kk
            if (Vx != KK) emu->pc += 2;
            break;
        }
        case 0x5000: { // 0x5XY0: skip next instruction if VX == VY
            if (Vx == Vy) emu->pc += 2;
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
            emulate_0x8XYN_opcode_cycle(emu, opcode);
            break;
        }
        case 0x9000: { // 0x9XY0: skip next instruction if VX != VY
            if (Vx != Vy) emu->pc += 2;
            break;
        }
        case 0xA000: { // 0xANNN: set index register to NNN
            emu->idx = NNN;
            break;
        }
        case 0xB000: { // 0xBNNN: jump to location V0 + NNN
            emu->pc = emu->V[0] + NNN;
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

            uint8_t& Vf = emu->V[0xF];
            Vf = 0;

            for (auto i = 0; i < N; i++) {
                uint8_t sprite_bits = emu->memory[emu->idx + i];
                const uint8_t y = (Vy + i) % chip8::DISPLAY_GRID_HEIGHT;

                uint8_t j = 7;
                while (sprite_bits != 0) {
                    const uint8_t x = (Vx + j) % chip8::DISPLAY_GRID_WIDTH;
                    bool& pixel_state = emu->gfx[y * chip8::DISPLAY_GRID_WIDTH + x];
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
            assert(Vx < chip8::USER_INPUT_KEY_COUNT);

            switch (opcode & 0x00FF) {
                case 0x009E: // 0xEX9E: skip next instruction if VXth key is pressed
                    assert(Vx < chip8::USER_INPUT_KEY_COUNT);
                    if (emu->input[Vx]) emu->pc += 2;
                    break;
                case 0x00A1: // 0xEXA1: skip next instruction if VXth key is NOT pressed
                    assert(Vx < chip8::USER_INPUT_KEY_COUNT);
                    if (!emu->input[Vx]) emu->pc += 2;
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

    if (bump_pc) emu->pc += 2;
    emu->cycles_emulated++;
}

bool requesting_beep(Emulator* emu) { return emu->sound_timer.read() > 0; }

// Unfortunatley we need our Emulator to be a global variable due to
// properly use the old C-style GLFW callbacks.
std::unique_ptr<Emulator> g_emu;

void key_callback(GLFWwindow* win, int key, int /* scancode */, int action, int /* mods */)
{
    if (action == GLFW_REPEAT) return;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
        return;
    }

    const bool state = action == GLFW_PRESS;

    int8_t key_id = -1;

    switch (key) {
        case GLFW_KEY_1:
            key_id = 0;
            break;
        case GLFW_KEY_2:
            key_id = 1;
            break;
        case GLFW_KEY_3:
            key_id = 2;
            break;
        case GLFW_KEY_4:
            key_id = 3;
            break;
        case GLFW_KEY_Q:
            key_id = 4;
            break;
        case GLFW_KEY_W:
            key_id = 5;
            break;
        case GLFW_KEY_E:
            key_id = 6;
            break;
        case GLFW_KEY_R:
            key_id = 7;
            break;
        case GLFW_KEY_A:
            key_id = 8;
            break;
        case GLFW_KEY_S:
            key_id = 9;
            break;
        case GLFW_KEY_D:
            key_id = 10;
            break;
        case GLFW_KEY_F:
            key_id = 11;
            break;
        case GLFW_KEY_Z:
            key_id = 12;
            break;
        case GLFW_KEY_X:
            key_id = 13;
            break;
        case GLFW_KEY_C:
            key_id = 14;
            break;
        case GLFW_KEY_V:
            key_id = 15;
            break;
    }

    if (key_id != -1) {
        if (g_emu->register_awaiting_input >= 0 && state && !g_emu->input[key_id]) {
            const auto X = g_emu->register_awaiting_input;
            g_emu->V[X] = key_id;
            g_emu->register_awaiting_input = -1;
            g_emu->pc += 2;
            g_emu->cycles_emulated++;
        }

        g_emu->input[key_id] = state;
    }
}

struct GraphicsContext {
    static constexpr auto GRID_CELL_PIXELS = 10;
    static constexpr auto screen_width_pixels = chip8::DISPLAY_GRID_WIDTH * GRID_CELL_PIXELS;
    static constexpr auto screen_height_pixels = chip8::DISPLAY_GRID_HEIGHT * GRID_CELL_PIXELS;

    GLFWwindow* emu_window = nullptr;
    const GLFWvidmode* glfw_video_mode = nullptr;

    static std::unique_ptr<GraphicsContext> create();

    ~GraphicsContext()
    {
        if (this->emu_window) glfwDestroyWindow(this->emu_window);
        glfwTerminate();
    }

private:
    GraphicsContext() = default;
};

std::unique_ptr<GraphicsContext> GraphicsContext::create()
{
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "Failed to initialize glfw!\n");
        return nullptr;
    }

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    static char window_name_buffer[64];

#ifdef _MSC_VER
    sprintf_s(window_name_buffer, 64, "CHIP-8 (version %d.%d)", CHIP8_VERSION_MAJOR,
              CHIP8_VERSION_MINOR);
#else
    sprintf(window_name_buffer, "CHIP-8 (version %d.%d)", CHIP8_VERSION_MAJOR,
            CHIP8_VERSION_MINOR);
#endif

    auto ctx = std::unique_ptr<GraphicsContext>(new GraphicsContext());

    ctx->emu_window = glfwCreateWindow(screen_width_pixels, screen_height_pixels,
                                       window_name_buffer, NULL, NULL);

    if (ctx->emu_window == nullptr) {
        return nullptr;
    }

    glfwMakeContextCurrent(ctx->emu_window);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glClearColor(0.34375, 0.29296875, 0.32421875, 1.0);

    const GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        return nullptr;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString(GL_VERSION);   // version as a string
    fprintf(stderr, "OpenGL Renderer Device: %s\n", renderer);
    fprintf(stderr, "OpenGL Version Supported %s\n", version);

    glfwSetKeyCallback(ctx->emu_window, key_callback);

    ctx->glfw_video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

    glfwSwapInterval(1); // enable vertical sync

    return ctx;
}

void draw_screen(Emulator* emu, GraphicsContext* gfx)
{
    constexpr auto gw = DISPLAY_GRID_WIDTH;
    constexpr auto gh = DISPLAY_GRID_HEIGHT;
    constexpr float grid_spacing_x = 2.f / gw;
    constexpr float grid_spacing_y = 2.f / gh;

    const bool* const gfx_buffer = emu->gfx;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(0.96484375, 0.62109375, 0.47265625);

    for (auto ix = 0; ix < chip8::DISPLAY_GRID_WIDTH; ix++) {
        for (auto iy = 0; iy < chip8::DISPLAY_GRID_HEIGHT; iy++) {
            if (gfx_buffer[iy * gw + ix]) {
                const float x0 = -1.f + ix * grid_spacing_x;
                const float y0 = 1.f - iy * grid_spacing_y;

                glBegin(GL_QUADS);
                {
                    glVertex2f(x0, y0);
                    glVertex2f(x0 + grid_spacing_x, y0);
                    glVertex2f(x0 + grid_spacing_x, y0 - grid_spacing_y);
                    glVertex2f(x0, y0 - grid_spacing_y);
                }
                glEnd();
            }
        }
    }

    glfwSwapBuffers(gfx->emu_window);
}

void main_loop(Emulator* emu, GraphicsContext* gfx, AudioContext* audio)
{
    bool currently_beeping = false;

    while (!glfwWindowShouldClose(gfx->emu_window)) {
        glfwPollEvents();

        for (auto i = 0; i < chip8::INSTRUCTIONS_PER_FRAME; i++) {
            emulate_cycle(emu);
        }

        if (requesting_beep(emu) && !currently_beeping) {
            start_beep(audio);
            currently_beeping = true;
        }
        else if (!requesting_beep(emu) && currently_beeping) {
            stop_beep(audio);
            currently_beeping = false;
        }

        draw_screen(emu, gfx);
    }
}

} // namespace chip8

int main(int argc, char* argv[])
{
    // seed the random number generator
    srand((unsigned)time(NULL));

    // clang-format off
    const std::string rom_path = ( fs::path(CHIP8_ASSETS_DIR)
                                 / fs::path("roms")
                                 / fs::path(argc > 1 ? argv[1] : "INVADERS")
                                 ).string();
    // clang-format on

    chip8::g_emu = chip8::Emulator::create(rom_path.c_str());
    if (!chip8::g_emu) return EXIT_FAILURE;

    auto gfx = chip8::GraphicsContext::create();
    if (!gfx) return EXIT_FAILURE;

    auto audio = chip8::AudioContext::create();
    if (!audio) return EXIT_FAILURE;

    chip8::main_loop(chip8::g_emu.get(), gfx.get(), audio.get());

    return EXIT_SUCCESS;
}
