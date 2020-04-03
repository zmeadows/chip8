#pragma once

#include <chrono>
using namespace std::chrono;

namespace chip8::timer {
// TODO: implement better timestep solution
// https://gafferongames.com/post/fix_your_timestep/

using clock =
    std::conditional<high_resolution_clock::is_steady, high_resolution_clock,
                     std::conditional<system_clock::period::den <= steady_clock::period::den,
                                      system_clock, steady_clock>::type>::type;

class cycle {
    const std::chrono::duration<double> cycle_duration;
    clock::time_point last_cycle_start;

public:
    cycle(double rate_Hz);
    bool is_ready(void);
    void wait_until_ready(void);
    void spin_until_ready(void);
};

} // namespace chip8::timer
