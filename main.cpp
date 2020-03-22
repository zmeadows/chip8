#include <chrono>

#include "chip8.hpp"

using namespace chip8;
using namespace std::chrono;

int main(void)
{
    setup_graphics();
    setup_input();

    emulator emu;
    load_game(&emu, "./roms/pong.rom");

    decltype(high_resolution_clock::now()) last_cycle_time;

    // TODO: implement better timestep solution
    // https://gafferongames.com/post/fix_your_timestep/
    while (true) {
        emulate_cycle(&emu);
        last_cycle_time = high_resolution_clock::now();

        if (emu.draw_flag) {
            draw_screen(&emu);
            emu.draw_flag = false;
        }

        read_key_state(&emu);

        while (true) {  // enforce 60Hz emulation cycle rate
            const auto right_now = high_resolution_clock::now();
            const duration<double> elapsed =
                duration_cast<duration<double>>(right_now - last_cycle_time);
            if (elapsed >= duration<double>{1.0 / 60.0}) break;
        }
    }

    return EXIT_SUCCESS;
};

