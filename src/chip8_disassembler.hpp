#pragma once

#include <cstdint>

namespace chip8::disassembler {

void init(const uint8_t* const rom_memory, const uint16_t* program_counter);

void terminate(void);

template <typename Callable>
void iterate_mru_window(Callable&& f);

} // namespace chip8::disassembler