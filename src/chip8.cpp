#include "chip8.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <thread>

#include "chip8_audio.hpp"
#include "chip8_emulator.hpp"
#include "chip8_glfw.hpp"
#include "chip8_timer.hpp"

namespace chip8 {

namespace {

constexpr auto shutdown_polling_delay = std::chrono::milliseconds(750);

void render_thread_func(void)
{
    chip8::glfw::init();

    // TODO: enable glfw vertical sync and remove this timer
    const auto rr = chip8::glfw::display_refresh_rate();
    chip8::timer::cycle draw_cycle((double)rr);

    const auto input_polling_delay = std::chrono::milliseconds(50);

    while (true) {
        if (chip8::glfw::shutdown_flag().load()) return;

        const bool draw_requested =
            chip8::emulator::draw_flag().wait_for(true, input_polling_delay);

        chip8::glfw::poll_user_input();

        if (draw_requested) {
            draw_cycle.wait_until_ready();
            chip8::glfw::draw_screen();
            chip8::emulator::draw_flag().unset();
        }
    }

    chip8::glfw::terminate();
}

void audio_thread_func(void)
{
chip8_poll_beep_start:
    if (chip8::glfw::shutdown_flag().load()) return;

    const bool beep_start_requested =
        chip8::emulator::beep_flag().wait_for(true, shutdown_polling_delay);

    if (beep_start_requested) {
        chip8::audio::start_beep();
        goto chip8_poll_beep_stop;
    }
    else {
        goto chip8_poll_beep_start;
    }

chip8_poll_beep_stop:
    if (chip8::glfw::shutdown_flag().load()) {
        chip8::audio::stop_beep();
        return;
    }

    const bool beep_stop_requested =
        chip8::emulator::beep_flag().wait_for(false, shutdown_polling_delay);

    if (beep_stop_requested) {
        chip8::audio::stop_beep();
        goto chip8_poll_beep_start;
    }
    else {
        goto chip8_poll_beep_stop;
    }
}

void emu_thread_func(void)
{
    chip8::timer::cycle emulation_cycle(540);
    while (!chip8::glfw::shutdown_flag().load()) {
        emulation_cycle.spin_until_ready();
        chip8::emulator::emulate_cycle();
    }
}

void timer_thread_func(void)
{
    chip8::timer::cycle timer_cycle(60);
    while (!chip8::glfw::shutdown_flag().load()) {
        timer_cycle.wait_until_ready();
        chip8::emulator::decrement_timers();
    }
}

} // namespace

void init(const char* rom_path, bool)
{
    srand((unsigned)time(NULL));
    chip8::audio::init();
    chip8::emulator::init(rom_path);
}

void terminate(void)
{
    assert(chip8::glfw::shutdown_flag().load());
    chip8::emulator::terminate();
    chip8::audio::terminate();
}

void run(void)
{
    std::thread emu_thread(emu_thread_func);
    std::thread audio_thread(audio_thread_func);
    std::thread timer_thread(timer_thread_func);
    std::thread render_thread(render_thread_func);

    // wait for shutdown
    while (true) {
        std::this_thread::sleep_for(shutdown_polling_delay);
        if (chip8::glfw::shutdown_flag().load()) break;
    }

    emu_thread.join();
    audio_thread.join();
    timer_thread.join();
    render_thread.join();
}

} // namespace chip8
