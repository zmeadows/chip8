#include <cstdio>
#include <cstdlib>

#include "cxxopts.hpp"

#include "chip8.hpp"

int main(int argc, char* argv[])
{
    cxxopts::Options options("chip8", "A CHIP-8 emulator written in C++.");

    // clang-format off
    options.add_options()
        ("d,debug", "Enable debugging")
        ("r,rom", "ROM to load", cxxopts::value<std::string>())
        ;
    // clang-format on

    auto opts = options.parse(argc, argv);

    const std::string local_rom_path =
        opts.count("rom") > 0 ? opts["rom"].as<std::string>() : "INVADERS";

    if (local_rom_path.size() > 2000) {
        fprintf(stderr, "un-reasonably long rom path entered!\n");
        return EXIT_FAILURE;
    }

    const auto ROM_PATH_BUFFER_LENGTH = 4096;
    char rom_path[ROM_PATH_BUFFER_LENGTH];

#ifdef _MSC_VER
    sprintf_s(rom_path, ROM_PATH_BUFFER_LENGTH, "%s/roms/%s", CHIP8_ASSETS_DIR,
              local_rom_path.c_str());
#else
    sprintf(rom_path, "%s/roms/%s", CHIP8_ASSETS_DIR, local_rom_path.c_str());
#endif

    chip8::init(rom_path, opts.count("debug") > 0);
    chip8::run();
    chip8::terminate();

    return EXIT_SUCCESS;
}
