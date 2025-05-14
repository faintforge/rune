#pragma once

#include "spire.h"
#include "rune/rune_tessellation.h"

typedef struct Renderer Renderer;
struct Renderer {
    u32 vao;
    u32 vbo;
    u32 ibo;
    u32 shader;
    u32 null_texture;

    RNE_Vertex vertex_buffer[4096];
    u16 index_buffer[4096];
};

extern Renderer renderer_create(void);
extern void renderer_destroy(Renderer* renderer);
extern void renderer_update_buffers(Renderer* renderer, u32 vertex_count, u32 index_count);
extern void renderer_update_projection(Renderer* renderer, SP_Ivec2 screen_size);
