#pragma once

#include "spire.h"

typedef struct Font Font;

typedef struct Glyph Glyph;
struct Glyph {
    SP_Vec2 size;
    SP_Vec2 offset;
    f32 advance;
    // [0] = Top left
    // [1] = Bottom right
    SP_Vec2 uv[2];
};

typedef struct FontMetrics FontMetrics;
struct FontMetrics {
    f32 ascent;
    f32 descent;
    f32 linegap;
};

extern Font*       font_create(SP_Arena* arena, SP_Str filename);
extern void        font_destroy(Font* font);
extern void        font_set_size(Font* font, u32 size);
extern Glyph       font_get_glyph(Font* font, u32 codepoint);
extern u32         font_get_atlas(const Font* font);
extern FontMetrics font_get_metrics(const Font* font);
extern f32         font_get_kerning(const Font* font, u32 left_codepoint, u32 right_codepoint);
extern SP_Vec2     font_measure_string(Font* font, SP_Str str);
