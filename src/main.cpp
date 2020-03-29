#include <cstdio>
#include <cstdlib>

#include "chip8.hpp"

int main(void)
{
    const auto ROM_PATH_BUFFER_LENGTH = 4096;
    char rom_path[ROM_PATH_BUFFER_LENGTH];
    sprintf_s(rom_path, ROM_PATH_BUFFER_LENGTH, "%s/roms/TETRIS", CHIP8_ASSETS_DIR);

    chip8::init(rom_path);
    chip8::run();
    chip8::terminate();

    return EXIT_SUCCESS;
}
