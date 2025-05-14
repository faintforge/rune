#pragma once

#include "rune.h"
#include "rune_tessellation.h"

typedef RNE_Handle RNE_UserData;

typedef RNE_UserData (*RNE_FontAtlasCreateFunc)(SP_Ivec2 size);
typedef void (*RNE_FontAtlasDestroyFunc)(RNE_UserData userdata);
typedef void (*RNE_FontAtlasResizeFunc)(RNE_UserData userdata, SP_Ivec2 size, const u8* pixels);
typedef void (*RNE_FontAtlasUpdateFunc)(RNE_UserData userdata, SP_Ivec2 pos, SP_Ivec2 size, u32 stride, const u8* pixels);

typedef struct RNE_FontCallbacks RNE_FontCallbacks;
struct RNE_FontCallbacks {
    RNE_FontAtlasCreateFunc create;
    RNE_FontAtlasDestroyFunc destroy;
    RNE_FontAtlasResizeFunc resize;
    RNE_FontAtlasUpdateFunc update;
};

extern RNE_Handle rne_font_create(SP_Arena* arena, SP_Str ttf_data, RNE_FontCallbacks callbacks);
extern void rne_font_destroy(RNE_Handle* font);

extern SP_Vec2 rne_text_measure(RNE_Handle font, SP_Str text, f32 size);
extern RNE_Glyph rne_font_get_glyph(RNE_Handle font, u32 codepoint, f32 size);
extern RNE_Handle rne_font_get_atlas(RNE_Handle font, f32 size);
extern RNE_FontMetrics rne_font_get_metrics(RNE_Handle font, f32 size);
extern f32 rne_font_get_kerning(RNE_Handle font, u32 left_codepoint, u32 right_codepoint, f32 size);

#define RNE_FONT_INTERFACE ((RNE_FontInterface) { \
    .get_glyph = rne_font_get_glyph, \
    .get_atlas = rne_font_get_atlas, \
    .get_metrics = rne_font_get_metrics, \
    .get_kerning = rne_font_get_kerning, \
})
