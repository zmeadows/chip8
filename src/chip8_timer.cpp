#include "chip8_timer.hpp"

#include <thread>

namespace chip8::timer {

cycle::cycle(double rate_Hz) : cycle_duration(1.0 / rate_Hz), last_cycle_start(clock::now()) {}

void cycle::wait_until_ready(void)
{
    std::this_thread::sleep_until(last_cycle_start + cycle_duration);
    last_cycle_start = chip8::timer::clock::now();
}

} // namespace chip8::timer