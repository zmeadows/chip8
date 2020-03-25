#include "chip8.hpp"

#include <cstdio>

int main(void)
{
    chip8::init();

    char rom_path[512];
    sprintf_s(rom_path, 512, "%s/roms/corax89_test/test_opcode.ch8", CHIP8_ASSETS_DIR);

    auto emu = chip8::core::create_emulator(rom_path);

    while (true) {
        const bool game_finished = chip8::tick(emu);
        if (game_finished) break;
    }

    chip8::terminate();

    return EXIT_SUCCESS;
};
