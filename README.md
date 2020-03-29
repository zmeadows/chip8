![](https://raw.githubusercontent.com/zmeadows/chip8/master/pong.PNG)

## BUILDING
The following cross-platform libraries are used (and included):

glew
glfw
miniaudio
imgui

Follow the typical cmake build procedure:

``
mkdir build
cd build
cmake ..
make -j 4
``

## INPUT

This emulator uses the following 16 key block for input:

``
1234
QWER
ASDF
ZXCV
``