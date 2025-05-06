#pragma once

#include "rune.h"

typedef struct RNE_Glyph RNE_Glyph;
struct RNE_Glyph {
    SP_Vec2 size;
    SP_Vec2 offset;
    f32 advance;
    // [0] = Top left
    // [1] = Bottom right
    SP_Vec2 uv[2];
};

typedef struct RNE_FontMetrics RNE_FontMetrics;
struct RNE_FontMetrics {
    f32 ascent;
    f32 descent;
    f32 linegap;
};

typedef RNE_Glyph (*RNE_FontGetGlyphFunc)(RNE_Handle font, u32 codepoint, f32 size);
typedef RNE_Handle (*RNE_FontGetAtlasFunc)(RNE_Handle font, f32 size);
typedef RNE_FontMetrics (*RNE_FontGetMetricsFunc)(RNE_Handle font, f32 size);
typedef f32 (*RNE_FontGetKerningFunc)(RNE_Handle font, u32 left_codepoint, u32 right_codepoint, f32 size);

typedef struct RNE_FontInterface RNE_FontInterface;
struct RNE_FontInterface {
    RNE_FontGetGlyphFunc get_glyph;
    RNE_FontGetAtlasFunc get_atlas;
    RNE_FontGetMetricsFunc get_metrics;
    // Can be left as null.
    RNE_FontGetKerningFunc get_kerning;
};

typedef struct RNE_Vertex RNE_Vertex;
struct RNE_Vertex {
    SP_Vec2 pos;
    SP_Vec2 uv;
    SP_Color color;
    u32 texture_index;
};

typedef struct RNE_TessellationConfig RNE_TessellationConfig;
struct RNE_TessellationConfig {
    SP_Arena* arena;
    RNE_FontInterface font;

    RNE_Vertex* vertex_buffer;
    u32 vertex_capacity;

    u16* index_buffer;
    u32 index_capacity;

    RNE_Handle* texture_buffer;
    u32 texture_capacity;
    RNE_Handle null_texture;
};

typedef struct RNE_RenderCmd RNE_RenderCmd;
struct RNE_RenderCmd {
    RNE_RenderCmd* next;

    u32 start_offset_bytes;
    u32 index_count;
    RNE_DrawScissor scissor;
};

typedef struct RNE_BatchCmd RNE_BatchCmd;
struct RNE_BatchCmd {
    u32 vertex_count;
    u32 index_count;
    u32 texture_count;
    RNE_RenderCmd* render_cmds;
};

typedef struct RNE_TessellationState RNE_TessellationState;
struct RNE_TessellationState {
    b8 finished;
    b8 not_first_call;
    RNE_DrawCmd* current_cmd;
    RNE_DrawScissor current_scissor;
};

extern RNE_BatchCmd rne_tessellate(RNE_DrawCmdBuffer* buffer,
        RNE_TessellationConfig config,
        RNE_TessellationState* state);
