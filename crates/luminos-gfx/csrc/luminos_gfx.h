#ifndef LUMINOS_GFX_H
#define LUMINOS_GFX_H

#include <stdint.h>

typedef struct LuminosWindow LuminosWindow;

typedef enum {
    LUMINOS_BACKEND_WAYLAND = 0,
    LUMINOS_BACKEND_COTANOS = 1
} LuminosBackend;

LuminosWindow* luminos_window_create(LuminosBackend backend, const char* title, int width, int height);
int luminos_window_should_close(LuminosWindow* window);
void luminos_window_swap_buffers(LuminosWindow* window);
void luminos_window_clear(LuminosWindow* window, float r, float g, float b);
int luminos_window_poll_events(LuminosWindow* window);
void luminos_window_destroy(LuminosWindow* window);

#endif // LUMINOS_GFX_H
