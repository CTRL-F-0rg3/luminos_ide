#include <iostream>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <wayland-client.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>

extern "C" {
    #include "xdg-shell-client-protocol.h"
}

#include "luminos_protocol.h"

#define SOCKET_PATH "/tmp/luminos_compositor.sock"

// --- Globalne listenery Wayland (poza klasą, aby uniknąć problemów z inicjalizacją w C++) ---
static void wm_base_ping_handler(void* data, xdg_wm_base* wm_base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(wm_base, serial);
}
static const xdg_wm_base_listener wm_base_listener = { wm_base_ping_handler };

static void xdg_surface_configure_handler(void* data, xdg_surface* xdg_surf, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(xdg_surf, serial);
}
static const xdg_surface_listener xdg_surf_listener = {
    .configure = xdg_surface_configure_handler
};
// --------------------------------------------------------------------------------------------

struct TranslatedWindow {
    wl_surface* surface;
    xdg_surface* xdg_surf;
    xdg_toplevel* xdg_top;
    int width;
    int height;
    wl_region* opaque_region;
};

class LuminosTranslator {
private:
    wl_display* display;
    wl_registry* registry;
    wl_compositor* compositor;
    xdg_wm_base* wm_base;
    
    std::unordered_map<uint32_t, TranslatedWindow> windows;
    int server_fd = -1;
    int client_fd = -1;

    static void registry_global_handler(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
        auto* self = static_cast<LuminosTranslator*>(data);
        if (std::strcmp(interface, "wl_compositor") == 0) {
            self->compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 1));
        } else if (std::strcmp(interface, "xdg_wm_base") == 0) {
            self->wm_base = static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        }
    }

    static void registry_global_remove_handler(void* data, wl_registry* registry, uint32_t name) {
        (void)data; (void)registry; (void)name;
    }

    bool setup_socket() {
        unlink(SOCKET_PATH);
        server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd < 0) return false;

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return false;
        listen(server_fd, 1);
        std::cout << "[COMPOSITOR] Nasłuchuję na: " << SOCKET_PATH << "\n";
        return true;
    }

public:
    LuminosTranslator() : display(nullptr), registry(nullptr), compositor(nullptr), wm_base(nullptr) {}

    bool connect_to_host_wayland() {
        display = wl_display_connect(nullptr);
        if (!display) return false;

        registry = wl_display_get_registry(display);
        static const wl_registry_listener registry_listener = { registry_global_handler, registry_global_remove_handler };
        wl_registry_add_listener(registry, &registry_listener, this);
        
        wl_display_dispatch(display);
        wl_display_roundtrip(display);

        if (!compositor || !wm_base) return false;

        xdg_wm_base_add_listener(wm_base, &wm_base_listener, nullptr);

        return setup_socket();
    }

    void process_cotanos_message(const LuminosHeader* header, const void* payload) {
        std::cout << "[COMPOSITOR] Odebrano: type=" << header->type << ", surface_id=" << header->surface_id << "\n";

        switch (header->type) {
            case MSG_CREATE_SURFACE: {
                TranslatedWindow win;
                win.surface = wl_compositor_create_surface(compositor);
                
                win.xdg_surf = xdg_wm_base_get_xdg_surface(wm_base, win.surface);
                xdg_surface_add_listener(win.xdg_surf, &xdg_surf_listener, nullptr); // Używamy globalnego listenera
                
                win.xdg_top = xdg_surface_get_toplevel(win.xdg_surf);
                xdg_toplevel_set_title(win.xdg_top, "Luminos IDE (Live!)");
                
                wl_surface_commit(win.surface);
                
                win.opaque_region = nullptr;
                windows[header->surface_id] = win;
                std::cout << "  -> Okno utworzone i zatwierdzone.\n";
                break;
            }
            case MSG_ATTACH_BUFFER: {
                const BufferPayload* buf = static_cast<const BufferPayload*>(payload);
                auto it = windows.find(header->surface_id);
                if (it != windows.end()) {
                    std::cout << "  -> Buffer: " << buf->width << "x" << buf->height << " (FD: " << buf->dma_buf_fd << ")\n";
                    it->second.width = buf->width;
                    it->second.height = buf->height;
                }
                break;
            }
            case MSG_SET_OPAQUE_REGION: {
                size_t num_rects = header->payload_size / sizeof(LuminosRect);
                const LuminosRect* rects = static_cast<const LuminosRect*>(payload);
                auto it = windows.find(header->surface_id);
                if (it != windows.end() && compositor) {
                    if (it->second.opaque_region) wl_region_destroy(it->second.opaque_region);
                    it->second.opaque_region = wl_compositor_create_region(compositor);
                    for (size_t i = 0; i < num_rects; ++i) {
                        wl_region_add(it->second.opaque_region, rects[i].x, rects[i].y, rects[i].width, rects[i].height);
                    }
                    wl_surface_set_opaque_region(it->second.surface, it->second.opaque_region);
                    std::cout << "  -> Ustawiono regiony nieprzezroczyste.\n";
                }
                break;
            }
            case MSG_COMMIT: {
                auto it = windows.find(header->surface_id);
                if (it != windows.end()) {
                    wl_surface_commit(it->second.surface);
                    std::cout << "  -> wl_surface_commit (Okno powinno być widoczne!).\n";
                }
                break;
            }
            case MSG_DESTROY: {
                auto it = windows.find(header->surface_id);
                if (it != windows.end()) {
                    if (it->second.opaque_region) wl_region_destroy(it->second.opaque_region);
                    if (it->second.xdg_top) xdg_toplevel_destroy(it->second.xdg_top);
                    if (it->second.xdg_surf) xdg_surface_destroy(it->second.xdg_surf);
                    if (it->second.surface) wl_surface_destroy(it->second.surface);
                    windows.erase(it);
                    std::cout << "  -> Okno zniszczone.\n";
                }
                break;
            }
        }
    }

    void run_event_loop() {
        std::cout << "[COMPOSITOR] Uruchamiam główną pętlę (Ctrl+C aby zakończyć)...\n";
        
        struct pollfd fds[2];
        fds[0].fd = wl_display_get_fd(display);
        fds[0].events = POLLIN;
        fds[1].fd = server_fd;
        fds[1].events = POLLIN;

        while (true) {
            if (poll(fds, 2, -1) < 0) break;

            if (fds[1].revents & POLLIN) {
                if (client_fd < 0) {
                    client_fd = accept(server_fd, nullptr, nullptr);
                    std::cout << "[COMPOSITOR] IDE połączyło się!\n";
                }
            }

            if (client_fd >= 0) {
                LuminosHeader header;
                ssize_t bytes = recv(client_fd, &header, sizeof(header), MSG_DONTWAIT);
                if (bytes == sizeof(header)) {
                    std::vector<char> payload(header.payload_size);
                    if (header.payload_size > 0) {
                        recv(client_fd, payload.data(), header.payload_size, 0);
                    }
                    process_cotanos_message(&header, payload.data());
                } else if (bytes == 0) {
                    std::cout << "[COMPOSITOR] IDE rozłączyło się. Zamykam kompozytor.\n";
                    close(client_fd);
                    client_fd = -1;
                    break;
                }
            }

            if (fds[0].revents & POLLIN) {
                wl_display_dispatch(display);
            }
        }
    }

    ~LuminosTranslator() {
        if (client_fd >= 0) close(client_fd);
        if (server_fd >= 0) {
            close(server_fd);
            unlink(SOCKET_PATH);
        }
        if (display) wl_display_disconnect(display);
    }
};

int main() {
    LuminosTranslator translator;
    if (!translator.connect_to_host_wayland()) {
        std::cerr << "Nie udało się zainicjalizować kompozytora.\n";
        return 1;
    }
    translator.run_event_loop();
    return 0;
}
