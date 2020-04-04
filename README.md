![](https://raw.githubusercontent.com/zmeadows/chip8/master/pong.PNG)

## BUILDING

Follow the typical cmake build procedure:

```
mkdir build
cd build
cmake ..
make -j 4
```

The following cross-platform libraries are used (and included):

* glew [http://glew.sourceforge.net/]
* glfw [https://www.glfw.org/]
* miniaudio [https://miniaud.io/]
* imgui [https://github.com/ocornut/imgui]
* cxxopts [https://github.com/jarro2783/cxxopts]

## INPUT

This emulator uses a square block of 16 keyboard keys for input:

```
1234
QWER
ASDF
ZXCV
```

## CODE OVERVIEW

Emulators tend to de-couple nicely into various modules with their own local state. In this implementation, modules are relegated to separate namespaces/files and the local state is kept within an anonymous namespaces inside each module. This is akin to static variables in C. If it helps you can imagine each module as akin to a 'singleton' class in more traditional OOP nomenclature.

## VERSION HISTORY

#### 0.1
Emulator passes all tests in BC_test and corax89.
OpenGL/glew/glfw screen display working correctly.
No sound or memory/register/disassembly visualization yet.

#### 0.2
Beep sound effect now included (thanks to miniaudio!).

#### 0.3
Multi-threaded support added for handling rendering, audio, timers and instruction cycles in four separate threads.

#### 0.4
Multi-threaded support removed (it wasn't necessary). Simplified main loop and user input handling.