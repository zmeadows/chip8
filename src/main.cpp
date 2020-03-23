#include <chrono>

#include "chip8.hpp"

int main(void)
{
    chip8::init();

    chip8::core::emulator emu = chip8::create_emulator("./roms/pong.rom");

    while (true) {
        const bool game_finished = chip8::tick(emu);
        if (game_finished) break;
    }

    chip8::terminate();

    return EXIT_SUCCESS;
};

