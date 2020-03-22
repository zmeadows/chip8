FLAGS="-std=c++2a -g -march=native -Wall -Wextra -fno-exceptions -fno-rtti -pedantic -Wno-narrowing -Werror -Wno-unused-parameter -Wno-unused-variable"

clang++-10 main.cpp -o chip8 ${FLAGS} && ./chip8
