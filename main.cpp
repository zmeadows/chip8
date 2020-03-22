#include "chip8.hpp"

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

