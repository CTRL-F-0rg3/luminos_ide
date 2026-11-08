#ifndef LUMINOS_PROTOCOL_H
#define LUMINOS_PROTOCOL_H

#include <stdint.h>

typedef enum {
    MSG_CREATE_SURFACE = 1,
    MSG_ATTACH_BUFFER  = 2,
    MSG_COMMIT         = 3,
    MSG_DESTROY        = 4,
    MSG_SET_TITLE      = 5,
    MSG_SET_OPAQUE_REGION = 6 // NOWE: Definiuje, które części okna są NIEprzezroczyste
} LuminosMsgType;

typedef struct {
    uint32_t surface_id;
    LuminosMsgType type;
    uint32_t payload_size;
} LuminosHeader;

typedef struct {
    int32_t dma_buf_fd;
    int32_t width;
    int32_t height;
    uint32_t format;
    uint32_t stride;
} BufferPayload;

// NOWE: Prosty prostokąt dla regionów
typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} LuminosRect;

#endif // LUMINOS_PROTOCOL_H
