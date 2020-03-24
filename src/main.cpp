#include "chip8.hpp"
#include "chip8_core.hpp"
#include "chip8_glfw.hpp"

#include <cstdio>

int main(void)
{
    chip8::init();

    auto emu = chip8::core::create_emulator("./roms/pong.rom");

    while (true) {
        const bool game_finished = chip8::tick(emu);
        if (game_finished) break;
    }

    chip8::terminate();

    return EXIT_SUCCESS;
};
