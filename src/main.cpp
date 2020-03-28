#include <cstdio>
#include <cstdlib>

#include "chip8.hpp"

int main(void)
{
    char rom_path[512];
    sprintf_s(rom_path, 512, "%s/roms/corax89_test/test_opcode.ch8", CHIP8_ASSETS_DIR);

    chip8::init(rom_path);
    chip8::run();
    chip8::terminate();

    return EXIT_SUCCESS;
}
