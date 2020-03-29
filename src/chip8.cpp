#include "chip8.hpp"

#include <cstdlib>
#include <ctime>

#include "chip8_audio.hpp"
#include "chip8_emulator.hpp"
#include "chip8_glfw.hpp"
#include "chip8_timer.hpp"

namespace chip8 {

void init(const char* rom_path, bool)
{
    srand((unsigned)time(NULL));
    chip8::glfw::init();
    chip8::audio::init();
    chip8::emulator::init(rom_path);
}

void terminate(void)
{
    chip8::audio::terminate();
    chip8::glfw::terminate();
}

namespace {

// TODO: detect monitor refresh rate
struct chip8::timer::cycle draw_cycle = chip8::timer::create_cycle(144);
struct chip8::timer::cycle emulation_cycle = chip8::timer::create_cycle(600);
struct chip8::timer::cycle delay_sound_cycle = chip8::timer::create_cycle(60);

bool beeping = false;

} // namespace

void run(void)
{
    while (true) {
        if (chip8::glfw::user_requested_window_close()) return;

        if (chip8::timer::is_ready(delay_sound_cycle)) {
            chip8::emulator::decrement_timers();
        }

        if (chip8::timer::is_ready(emulation_cycle)) {
            chip8::glfw::poll_user_input();
            chip8::emulator::emulate_cycle();
        }

        if (!beeping && chip8::emulator::is_beeping()) {
            beeping = true;
            chip8::audio::start_beep();
        }
        else if (beeping && !chip8::emulator::is_beeping()) {
            beeping = false;
            chip8::audio::end_beep();
        }

        // TODO: ensure glfw isn't blocking on this call and the
        // timing is happening the way it should be
        if (chip8::emulator::screen_state_changed() && chip8::timer::is_ready(draw_cycle)) {
            chip8::glfw::draw_screen();
            chip8::emulator::reset_draw_flag();
        }
    }
}

} // namespace chip8
