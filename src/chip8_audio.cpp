#include "chip8_audio.hpp"

#define DR_WAV_IMPLEMENTATION
#include "miniaudio/extras/dr_wav.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

namespace chip8::audio {

namespace {

}

void init(void) {}
void terminate(void) {}
void start_beep(void) {}
void end_beep(void) {}

} // namespace chip8::audio