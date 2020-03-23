#include "chip8.hpp"

#include "chip8_core.hpp"
#include "chip8_glfw.hpp"

namespace chip8 {

void init(void) { chip8::glfw::init(); }

void terminate(void) { chip8::glfw::terminate(); }

struct chip8::core::emulator create_emulator(const char* rom_path)
{
    FILE* infile = fopen(rom_path, "rb");

    if (infile == nullptr) {
        printf("failed to load ROM from file: %s\n", rom_path);
        exit(EXIT_FAILURE);
    }

    fseek(infile, 0, SEEK_END);
    const auto ftell_ret = ftell(infile);
    rewind(infile);

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

    const uint64_t file_bytes_read = fread(buffer, 1, file_size_bytes, infile);
    if (file_bytes_read != file_size_bytes) {
        printf("error reading input rom file: %s\n", rom_path);
        free(buffer);
        exit(EXIT_FAILURE);
    }

    chip8::core::emulator emu;
    reset(emu);

    for (uint64_t i = 0; i < file_size_bytes; i++) {
        emu.memory[emulator::rom_memory_offset + i] = buffer[i];
    }

    printf("successfully loaded %lu bytes from %s into memory.\n\n", file_size_bytes,
           rom_path);

    free(buffer);

    return emu;
}

bool tick(struct chip8::core::emulator& emu)
{
    // TODO: implement better timestep solution
    // https://gafferongames.com/post/fix_your_timestep/

    while (true) {  // enforce 60Hz emulation cycle rate
        const auto right_now = chip8::core::clock::now();
        const duration<double> elapsed =
            duration_cast<duration<double>>(right_now - last_cycle_time);
        if (elapsed >= duration<double>{1.0 / 60.0}) break;
    }

    chip8::core::emulate_cycle(emu);

    if (emu.draw_flag) {
        chip8::glfw::draw_screen(emu);
        emu.draw_flag = false;
    }

    chip8::glfw::update_input_state(emu);

    return false;
}

}  // namespace chip8
