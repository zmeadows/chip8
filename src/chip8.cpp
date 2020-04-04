#include "chip8.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>

#include "chip8_audio.hpp"
#include "chip8_emulator.hpp"
#include "chip8_glfw.hpp"

namespace chip8 {

void init(const char* rom_path, bool)
{
    srand((unsigned)time(NULL));
    chip8::audio::init();
    chip8::emulator::init(rom_path);
    chip8::glfw::init();
}

void terminate(void)
{
    chip8::emulator::terminate();
    chip8::audio::terminate();
    chip8::glfw::terminate();
}

void run(void)
{
    constexpr auto instructions_per_frame = 5;

    static bool currently_beeping = false;

    while (true) {
        chip8::glfw::poll_user_input();

        for (auto i = 0; i < instructions_per_frame; i++) {
            chip8::emulator::emulate_cycle();
        }

        if (chip8::emulator::is_beeping() && !currently_beeping) {
            chip8::audio::start_beep();
            currently_beeping = true;
        }
        else if (!chip8::emulator::is_beeping() && currently_beeping) {
            chip8::audio::stop_beep();
            currently_beeping = false;
        }

        chip8::glfw::draw_screen();

        if (chip8::glfw::user_requested_window_close()) {
            break;
        }
    }
}

} // namespace chip8
