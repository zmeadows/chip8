FLAGS="-std=c++2a -g -march=native -Wall -Wextra -fno-exceptions -fno-rtti -pedantic -Wno-narrowing -Werror -Wno-unused-parameter -Wno-unused-variable"

clang++-10 main.cpp -o chip8 ${FLAGS}
echo "emulator build successful"
clang++-10 chip8_tests.cpp -o chip8_tests ${FLAGS}
echo "test build successful"
./chip8_tests
