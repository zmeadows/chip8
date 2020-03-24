#include "chip8_glfw.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "chip8_config.h"

namespace {

GLFWwindow* window = nullptr;

bool input_buffer[chip8::core::emulator::user_input_key_count] = {false};

void key_callback(GLFWwindow* win, int key, int /* scancode */, int action, int /* mods */)
{
    if (action == GLFW_REPEAT) return;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
        return;
    }

    const bool state = action == GLFW_PRESS;

    switch (key) {
        case GLFW_KEY_1:
            input_buffer[0] = state;
            break;
        case GLFW_KEY_2:
            input_buffer[1] = state;
            break;
        case GLFW_KEY_3:
            input_buffer[2] = state;
            break;
        case GLFW_KEY_4:
            input_buffer[3] = state;
            break;
        case GLFW_KEY_Q:
            input_buffer[4] = state;
            break;
        case GLFW_KEY_W:
            input_buffer[5] = state;
            break;
        case GLFW_KEY_E:
            input_buffer[6] = state;
            break;
        case GLFW_KEY_R:
            input_buffer[7] = state;
            break;
        case GLFW_KEY_A:
            input_buffer[8] = state;
            break;
        case GLFW_KEY_S:
            input_buffer[9] = state;
            break;
        case GLFW_KEY_D:
            input_buffer[10] = state;
            break;
        case GLFW_KEY_F:
            input_buffer[11] = state;
            break;
        case GLFW_KEY_Z:
            input_buffer[12] = state;
            break;
        case GLFW_KEY_X:
            input_buffer[13] = state;
            break;
        case GLFW_KEY_C:
            input_buffer[14] = state;
            break;
        case GLFW_KEY_V:
            input_buffer[15] = state;
            break;
    }
}

} // namespace

namespace chip8::glfw {

void init(void)
{
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "failed to initialize glfw!\n");
        exit(EXIT_FAILURE);
    }

    static char window_name_buffer[32];
    sprintf_s(window_name_buffer, 32, "CHIP-8 (version %d.%d)", CHIP8_VERSION_MAJOR,
              CHIP8_VERSION_MINOR);

    window = glfwCreateWindow(640, 320, window_name_buffer, NULL, NULL);

    if (window == nullptr) {
        glfwTerminate();
    }

    glfwSetKeyCallback(window, ::key_callback);

    glfwMakeContextCurrent(window);
}

void terminate(void) { glfwTerminate(); }

void draw_screen(const struct chip8::core::emulator&)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
}

void update_input_state(struct chip8::core::emulator& emu)
{
    glfwPollEvents();
    for (auto i = 0; i < chip8::core::emulator::user_input_key_count; i++) {
        emu.input[i] = ::input_buffer[i];
    }
}

bool user_requested_window_close(void) { return glfwWindowShouldClose(window); }

} // namespace chip8::glfw