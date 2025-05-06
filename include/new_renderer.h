#pragma once

#include "spire.h"
#include "rune.h"

typedef struct NewRenderer NewRenderer;
struct NewRenderer {
    u32 vao;
    u32 vbo;
    u32 ibo;
    u32 shader;

    RNE_Vertex vertex_buffer[4096];
    u16 index_buffer[4096];
};

extern NewRenderer new_renderer_create(void);
extern void new_renderer_destroy(NewRenderer* renderer);
extern void new_renderer_update_buffers(NewRenderer* renderer, u32 vertex_count, u32 index_count);
extern void new_renderer_update_projection(NewRenderer* renderer, SP_Ivec2 screen_size);
