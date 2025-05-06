#pragma once

#include "rune.h"

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

