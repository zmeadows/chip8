#include "chip8_glfw.hpp"

#include <cstdio>
#include <cstdlib>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "chip8_config.h"
#include "chip8_emulator.hpp"

namespace chip8::glfw {

namespace {

GLFWwindow* emu_window = nullptr;
GLFWwindow* debug_window = nullptr;

constexpr auto grid_cell_pixels = 10;
constexpr auto screen_width_pixels = emulator::display_grid_width * grid_cell_pixels;
constexpr auto screen_height_pixels = emulator::display_grid_height * grid_cell_pixels;

const GLFWvidmode* glfw_video_mode = nullptr;

bool input_buffer[emulator::user_input_key_count] = {false};

void key_callback(GLFWwindow* win, int key, int /* scancode */, int action, int /* mods */)
{
    if (action == GLFW_REPEAT) return;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
        return;
    }

    const bool state = action == GLFW_PRESS;

    int8_t key_id = -1;

    switch (key) {
        case GLFW_KEY_1:
            key_id = 0;
            break;
        case GLFW_KEY_2:
            key_id = 1;
            break;
        case GLFW_KEY_3:
            key_id = 2;
            break;
        case GLFW_KEY_4:
            key_id = 3;
            break;
        case GLFW_KEY_Q:
            key_id = 4;
            break;
        case GLFW_KEY_W:
            key_id = 5;
            break;
        case GLFW_KEY_E:
            key_id = 6;
            break;
        case GLFW_KEY_R:
            key_id = 7;
            break;
        case GLFW_KEY_A:
            key_id = 8;
            break;
        case GLFW_KEY_S:
            key_id = 9;
            break;
        case GLFW_KEY_D:
            key_id = 10;
            break;
        case GLFW_KEY_F:
            key_id = 11;
            break;
        case GLFW_KEY_Z:
            key_id = 12;
            break;
        case GLFW_KEY_X:
            key_id = 13;
            break;
        case GLFW_KEY_C:
            key_id = 14;
            break;
        case GLFW_KEY_V:
            key_id = 15;
            break;
    }

    if (key_id != -1) {
        // printf("key %x updated with state %d\n", key_id, state);
        input_buffer[key_id] = state;
    }
}

} // namespace

void init(void)
{
    if (glfwInit() != GLFW_TRUE) {
        fprintf(stderr, "failed to initialize glfw!\n");
        exit(EXIT_FAILURE);
    }

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    static char window_name_buffer[64];

#ifdef _MSC_VER
    sprintf_s(window_name_buffer, 64, "CHIP-8 (version %d.%d)", CHIP8_VERSION_MAJOR,
              CHIP8_VERSION_MINOR);
#else
    sprintf(window_name_buffer, "CHIP-8 (version %d.%d)", CHIP8_VERSION_MAJOR,
            CHIP8_VERSION_MINOR);
#endif

    emu_window = glfwCreateWindow(screen_width_pixels, screen_height_pixels,
                                  window_name_buffer, NULL, NULL);

    if (emu_window == nullptr) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(emu_window);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glClearColor(0.34375, 0.29296875, 0.32421875, 1.0);

    const GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW Error: %s\n", glewGetErrorString(err));
        exit(EXIT_FAILURE);
    }

    const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString(GL_VERSION);   // version as a string
    fprintf(stderr, "Renderer: %s\n", renderer);
    fprintf(stderr, "OpenGL version supported %s\n", version);

    glfwSetKeyCallback(emu_window, key_callback);
    // glfwSetWindowCloseCallback(emu_window, window_close_callback);

    glfw_video_mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

    glfwSwapInterval(1); // enable vertical sync
}

void terminate(void)
{
    glfwDestroyWindow(emu_window);
    glfwTerminate();
}

void draw_screen(void)
{
    constexpr auto gw = emulator::display_grid_width;
    constexpr auto gh = emulator::display_grid_height;
    constexpr float grid_spacing_x = 2.f / gw;
    constexpr float grid_spacing_y = 2.f / gh;

    const bool* const gfx_buffer = emulator::get_screen_state();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glColor3f(0.96484375, 0.62109375, 0.47265625);

    for (auto ix = 0; ix < emulator::display_grid_width; ix++) {
        for (auto iy = 0; iy < emulator::display_grid_height; iy++) {
            if (gfx_buffer[iy * gw + ix]) {
                const float x0 = -1.f + ix * grid_spacing_x;
                const float y0 = 1.f - iy * grid_spacing_y;

                glBegin(GL_QUADS);
                {
                    glVertex2f(x0, y0);
                    glVertex2f(x0 + grid_spacing_x, y0);
                    glVertex2f(x0 + grid_spacing_x, y0 - grid_spacing_y);
                    glVertex2f(x0, y0 - grid_spacing_y);
                }
                glEnd();
            }
        }
    }

    // glFlush();
    glfwSwapBuffers(emu_window);
}

void poll_user_input(void)
{
    glfwPollEvents();
    emulator::update_user_input(input_buffer);
}

bool user_requested_window_close(void)
{
    return glfwWindowShouldClose(emu_window); // || glfwWindowShouldClose(debug_window);
}

uint64_t display_width_pixels(void)
{
    const int w = glfw_video_mode->width;
    return w > 0 ? (uint64_t)w : 0;
}

uint64_t display_height_pixels(void)
{
    const int h = glfw_video_mode->height;
    return h > 0 ? (uint64_t)h : 0;
}

uint64_t display_refresh_rate(void)
{
    const int r = glfw_video_mode->refreshRate;
    return r > 0 ? (uint64_t)r : 0;
}

} // namespace chip8::glfw
