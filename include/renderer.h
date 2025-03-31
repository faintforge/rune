#pragma once

#include "spire.h"
#include "font.h"

typedef struct Vertex Vertex;
struct Vertex {
    SP_Vec2 pos;
    SP_Vec2 uv;
    SP_Vec4 color;
    u32 texture_index;
};

typedef struct Renderer Renderer;
struct Renderer {
    u32 vbo;
    u32 ibo;
    u32 vao;
    u32 shader;

    u32 curr_texture;
    u32 textures[32];
    u32 max_quads;
    u32 curr_quad;
    Vertex* vertices;
    SP_Mat4 projection;
};

typedef struct RenderBox RenderBox;
struct RenderBox {
    SP_Vec2 pos;
    SP_Vec2 size;
    SP_Vec4 color;
    u32 texture;
    b8 has_uv;
    SP_Vec2 uv[2];
};

extern Renderer renderer_create(SP_Arena* arena);
extern void renderer_destroy(Renderer* renderer);
extern void renderer_begin(Renderer* renderer, SP_Ivec2 screen_size);
extern void renderer_end(Renderer* renderer);
extern void renderer_draw(Renderer* renderer, RenderBox box);
extern void renderer_draw_text(Renderer* renderer, SP_Vec2 pos, SP_Str text, Font* font, u32 size, SP_Vec4 color);
