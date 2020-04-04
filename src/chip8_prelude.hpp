#pragma once

#include <chrono>
using namespace std::chrono;

namespace chip8 {

using clock =
    std::conditional<high_resolution_clock::is_steady, high_resolution_clock,
                     std::conditional<system_clock::period::den <= steady_clock::period::den,
                                      system_clock, steady_clock>::type>::type;

} // namespace chip8