#include "chip8_timer.hpp"

#include <thread>

namespace chip8::timer {

cycle::cycle(double rate_Hz) : cycle_duration(1.0 / rate_Hz), last_cycle_start(clock::now()) {}

void cycle::wait_until_ready(void)
{
    std::this_thread::sleep_until(last_cycle_start + cycle_duration);
    last_cycle_start = chip8::timer::clock::now();
}

void cycle::spin_until_ready(void)
{
    while (!is_ready()) {
        continue;
    }
    return;
}

bool cycle::is_ready(void)
{
    const clock::time_point now = clock::now();

    const auto elapsed = duration_cast<duration<double>>(now - last_cycle_start);

    bool ready = false;
    if (elapsed >= cycle_duration) {
        ready = true;
        last_cycle_start = now;
    }

    return ready;
}

} // namespace chip8::timer