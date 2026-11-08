module luminos_backend;

extern(C) {
    import core.stdc.stdio;
    import core.stdc.stdlib;
    import core.stdc.string;
}

// Definicja enum zgodna z Rust/C
extern(C) enum LuminosBackend : int {
    WAYLAND = 0,
    COTANOS = 1
}

// Nieprzezroczysta struktura, którą D będzie zarządzać
struct LuminosWindowD {
    int width;
    int height;
    int should_close;
}

// --- API eksportowane do C/Rusta ---

extern(C) LuminosWindowD* luminos_d_window_create(LuminosBackend backend, const(char)* title, int width, int height) {
    printf("[D-BACKEND] luminos_d_window_create wywołane! Tytuł: %s, Rozmiar: %dx%d\n", title, width, height);
    
    if (backend == LuminosBackend.COTANOS) {
        printf("[D-BACKEND] Inicjalizacja natywnej logiki CoTanOS/Wayland w D...\n");
        
        LuminosWindowD* win = cast(LuminosWindowD*) malloc(LuminosWindowD.sizeof);
        if (!win) return null;
        
        win.width = width;
        win.height = height;
        win.should_close = 0;
        
        printf("[D-BACKEND] Sukces! Zwracam wskaźnik: %p\n", win);
        return win;
    }
    
    return null;
}

extern(C) int luminos_d_window_should_close(LuminosWindowD* win) {
    if (!win) return 1;
    return win.should_close;
}

extern(C) void luminos_d_window_swap_buffers(LuminosWindowD* win) {
    if (!win) return;
    // Tutaj w przyszłości D wywoła wl_surface_commit lub zarządzanie DMA-BUF
}

extern(C) void luminos_d_window_destroy(LuminosWindowD* win) {
    if (!win) return;
    printf("[D-BACKEND] Niszczenie okna %p i zwalnianie pamięci.\n", win);
    free(win);
}
