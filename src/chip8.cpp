#include "chip8.hpp"

#include "chip8_core.hpp"
#include "chip8_glfw.hpp"

namespace chip8 {

void init(void) { chip8::glfw::init(); }

void terminate(void) { chip8::glfw::terminate(); }

bool tick(struct chip8::core::emulator& emu)
{
    // TODO: implement better timestep solution
    // https://gafferongames.com/post/fix_your_timestep/

    while (true) { // enforce 60Hz emulation cycle rate
        const auto right_now = chip8::clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
            right_now - emu.last_cycle);
        if (elapsed >= std::chrono::duration<double>{1.0 / 60.0}) break;
    }

    chip8::core::emulate_cycle(emu);

    emu.draw_flag = true;
    if (emu.draw_flag) {
        chip8::glfw::draw_screen(emu);
        emu.draw_flag = false;
    }

    chip8::glfw::update_input_state(emu);

    if (chip8::glfw::user_requested_window_close()) return true;

    return false;
}

} // namespace chip8
