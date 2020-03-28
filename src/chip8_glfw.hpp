#pragma once

namespace chip8::glfw {

void init(void);
void terminate(void);
void draw_screen(void);
void poll_user_input(void);
bool user_requested_window_close(void);

} // namespace chip8::glfw
