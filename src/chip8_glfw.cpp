#include "chip8_glfw.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "chip8_config.h"

namespace {

GLFWwindow *window = nullptr;

}

namespace chip8::glfw {

void init(void) {
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "failed to initialize glfw!\n");
        exit(EXIT_FAILURE);
    }

    static char window_name_buffer[32];
    sprintf(window_name_buffer, "CHIP-8 (version %d.%d)",
            CHIP8_VERSION_MAJOR, CHIP8_VERSION_MINOR);

    window = glfwCreateWindow(640, 320, window_name_buffer, NULL, NULL);

    if (window == nullptr) {
        glfwTerminate();
    }

    glfwMakeContextCurrent(window);
}

void terminate(void) {
    glfwTerminate();
}

void draw_screen(const struct chip8::core::emulator &emu)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
}

void update_input_state(const struct chip8::core::emulator &emu)
{
    glfwPollEvents();
}

bool user_requested_window_close(void) {
    return glfwWindowShouldClose(window);
}

}