#include "chip8_timer.hpp"

namespace chip8::timer {

// TODO: implement better timestep solution
// https://gafferongames.com/post/fix_your_timestep/

struct cycle create_cycle(const double rate_Hz)
{
    return cycle{.cycle_duration = duration<double>{1.0 / rate_Hz},
                 .last_cycle_start = clock::now()};
}

bool is_ready(struct cycle& timer)
{
    const clock::time_point now = clock::now();

    const auto elapsed = duration_cast<duration<double>>(now - timer.last_cycle_start);

    bool ready = false;
    if (elapsed >= timer.cycle_duration) {
        ready = true;
        timer.last_cycle_start = now;
    }

    return ready;
}

} // namespace chip8::timer