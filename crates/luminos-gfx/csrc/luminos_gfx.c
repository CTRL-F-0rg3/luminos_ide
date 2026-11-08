#define _POSIX_C_SOURCE 199309L
#include "luminos_gfx.h"
#include <wayland-client.h>
#include <wayland-egl.h>
#include <xdg-shell-client-protocol.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

// Deklaracja funkcji z D (skompilowanych jako extern C)
struct LuminosWindowD;
struct LuminosWindowD* luminos_d_window_create(int backend, const char* title, int width, int height);
int luminos_d_window_should_close(struct LuminosWindowD* win);
void luminos_d_window_swap_buffers(struct LuminosWindowD* win);
void luminos_d_window_destroy(struct LuminosWindowD* win);

struct LuminosWindow {
    LuminosBackend backend_type;
    struct LuminosWindowD* d_window; // Wskaźnik do obiektu zarządzanego przez D
    
    struct wl_display* display;
    struct wl_registry* registry;
    struct wl_compositor* compositor;
    struct wl_surface* surface;
    struct xdg_wm_base* wm_base;
    struct xdg_surface* xdg_surface;
    struct xdg_toplevel* xdg_toplevel;
    struct wl_egl_window* egl_window;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    
    int should_close;
    int width, height;
};

// ... (Funkcje helpery Wayland pozostają bez zmian) ...
static void wm_base_ping_handler(void* data, struct xdg_wm_base* wm_base, uint32_t serial) { (void)data; xdg_wm_base_pong(wm_base, serial); }
static const struct xdg_wm_base_listener wm_base_listener = { .ping = wm_base_ping_handler };
static void xdg_surface_configure_handler(void* data, struct xdg_surface* xdg_surface, uint32_t serial) { (void)data; xdg_surface_ack_configure(xdg_surface, serial); }
static const struct xdg_surface_listener xdg_surface_listener = { .configure = xdg_surface_configure_handler };
static void xdg_toplevel_configure_handler(void* data, struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height, struct wl_array* states) {
    struct LuminosWindow* window = (struct LuminosWindow*)data;
    (void)xdg_toplevel; (void)states;
    if (width > 0 && height > 0) { window->width = width; window->height = height; if (window->egl_window) wl_egl_window_resize(window->egl_window, width, height, 0, 0); }
}
static void xdg_toplevel_close_handler(void* data, struct xdg_toplevel* xdg_toplevel) { struct LuminosWindow* window = (struct LuminosWindow*)data; (void)xdg_toplevel; window->should_close = 1; }
static const struct xdg_toplevel_listener xdg_toplevel_listener = { .configure = xdg_toplevel_configure_handler, .close = xdg_toplevel_close_handler };
static void registry_handler(void* data, struct wl_registry* registry, uint32_t id, const char* interface, uint32_t version) {
    struct LuminosWindow* window = (struct LuminosWindow*)data; (void)version;
    if (strcmp(interface, "wl_compositor") == 0) window->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    else if (strcmp(interface, "xdg_wm_base") == 0) { window->wm_base = wl_registry_bind(registry, id, &xdg_wm_base_interface, 1); xdg_wm_base_add_listener(window->wm_base, &wm_base_listener, window); }
}
static void registry_remover(void* data, struct wl_registry* registry, uint32_t id) { (void)data; (void)registry; (void)id; }
static const struct wl_registry_listener registry_listener = { registry_handler, registry_remover };

static LuminosWindow* init_wayland_backend(struct LuminosWindow* window, const char* title, int width, int height) {
    window->display = wl_display_connect(NULL);
    if (!window->display) { fprintf(stderr, "[LUMINOS] BŁĄD: Nie udało się połączyć z Wayland.\n"); return NULL; }
    window->registry = wl_display_get_registry(window->display);
    wl_registry_add_listener(window->registry, &registry_listener, window);
    wl_display_dispatch(window->display); wl_display_roundtrip(window->display);
    if (!window->compositor || !window->wm_base) return NULL;
    window->surface = wl_compositor_create_surface(window->compositor);
    window->xdg_surface = xdg_wm_base_get_xdg_surface(window->wm_base, window->surface);
    xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);
    window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
    xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);
    xdg_toplevel_set_title(window->xdg_toplevel, title);
    wl_surface_commit(window->surface);
    window->egl_window = wl_egl_window_create(window->surface, width, height);
    window->egl_display = eglGetDisplay((EGLNativeDisplayType)window->display);
    EGLint major, minor;
    eglInitialize(window->egl_display, &major, &minor);
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint config_attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    EGLConfig config; EGLint num_configs;
    eglChooseConfig(window->egl_display, config_attribs, &config, 1, &num_configs);
    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    window->egl_context = eglCreateContext(window->egl_display, config, EGL_NO_CONTEXT, context_attribs);
    window->egl_surface = eglCreateWindowSurface(window->egl_display, config, (EGLNativeWindowType)window->egl_window, NULL);
    eglMakeCurrent(window->egl_display, window->egl_surface, window->egl_surface, window->egl_context);
    printf("[LUMINOS] Wayland Backend: Sukces!\n");
    return window;
}

static LuminosWindow* init_cotanos_backend(struct LuminosWindow* window, const char* title, int width, int height) {
    printf("[LUMINOS] CoTanOS Backend: Delegowanie do biblioteki D...\n");
    window->d_window = luminos_d_window_create(LUMINOS_BACKEND_COTANOS, title, width, height);
    
    if (!window->d_window) {
        fprintf(stderr, "[LUMINOS] BŁĄD: Backend D zwrócił null.\n");
        return NULL;
    }
    printf("[LUMINOS] CoTanOS Backend: Sukces! D zainicjalizowało okno.\n");
    return window;
}

LuminosWindow* luminos_window_create(LuminosBackend backend, const char* title, int width, int height) {
    struct LuminosWindow* window = (struct LuminosWindow*)calloc(1, sizeof(struct LuminosWindow));
    if (!window) return NULL;
    window->backend_type = backend;
    window->width = width;
    window->height = height;

    if (backend == LUMINOS_BACKEND_WAYLAND) {
        if (!init_wayland_backend(window, title, width, height)) { free(window); return NULL; }
    } else if (backend == LUMINOS_BACKEND_COTANOS) {
        if (!init_cotanos_backend(window, title, width, height)) { free(window); return NULL; }
    } else {
        free(window); return NULL;
    }
    return (LuminosWindow*)window;
}

int luminos_window_should_close(LuminosWindow* win) { 
    if (!win) return 1;
    if (win->backend_type == LUMINOS_BACKEND_COTANOS) return luminos_d_window_should_close(win->d_window);
    return win->should_close; 
}

void luminos_window_swap_buffers(LuminosWindow* win) { 
    if (win && win->backend_type == LUMINOS_BACKEND_WAYLAND) {
        eglSwapBuffers(win->egl_display, win->egl_surface);
    } else if (win && win->backend_type == LUMINOS_BACKEND_COTANOS) {
        luminos_d_window_swap_buffers(win->d_window);
    }
}

void luminos_window_clear(LuminosWindow* win, float r, float g, float b) {
    if (win && win->backend_type == LUMINOS_BACKEND_WAYLAND) {
        glClearColor(r, g, b, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
    }
}

int luminos_window_poll_events(LuminosWindow* win) {
    if (!win) return 1;
    if (win->backend_type == LUMINOS_BACKEND_WAYLAND) {
        if (wl_display_dispatch_pending(win->display) < 0 || wl_display_flush(win->display) < 0) {
            win->should_close = 1; return 1;
        }
    } else if (win->backend_type == LUMINOS_BACKEND_COTANOS) {
        struct timespec ts = {0, 16000000};
        nanosleep(&ts, NULL);
    }
    return 0;
}

void luminos_window_destroy(LuminosWindow* win) {
    if (!win) return;
    struct LuminosWindow* window = (struct LuminosWindow*)win;
    
    if (window->backend_type == LUMINOS_BACKEND_COTANOS) {
        luminos_d_window_destroy(window->d_window);
    } else if (window->backend_type == LUMINOS_BACKEND_WAYLAND) {
        if (window->egl_display != EGL_NO_DISPLAY) {
            eglMakeCurrent(window->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (window->egl_surface != EGL_NO_SURFACE) eglDestroySurface(window->egl_display, window->egl_surface);
            if (window->egl_context != EGL_NO_CONTEXT) eglDestroyContext(window->egl_display, window->egl_context);
            eglTerminate(window->egl_display);
        }
        if (window->egl_window) wl_egl_window_destroy(window->egl_window);
        if (window->xdg_toplevel) xdg_toplevel_destroy(window->xdg_toplevel);
        if (window->xdg_surface) xdg_surface_destroy(window->xdg_surface);
        if (window->surface) wl_surface_destroy(window->surface);
        if (window->wm_base) xdg_wm_base_destroy(window->wm_base);
        if (window->compositor) wl_compositor_destroy(window->compositor);
        if (window->registry) wl_registry_destroy(window->registry);
        if (window->display) wl_display_disconnect(window->display);
    }
    free(window);
}
