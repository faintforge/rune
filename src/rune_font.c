#include "rune_font.h"
#include "spire.h"

#include <stb_truetype.h>

#include <string.h>

// -- Quadtree packer ----------------------------------------------------------

typedef struct QuadtreeAtlasNode QuadtreeAtlasNode;
struct QuadtreeAtlasNode {
    QuadtreeAtlasNode* children;
    SP_Ivec2 pos;
    SP_Ivec2 size;
    b8 occupied;
    b8 split;
};

typedef struct QuadtreeAtlas QuadtreeAtlas;
struct QuadtreeAtlas {
    SP_Arena* arena;
    QuadtreeAtlasNode root;
    SP_Ivec2 size;
    u8* bitmap;
};

static QuadtreeAtlas quadtree_atlas_init(SP_Arena* arena, SP_Ivec2 size) {
    QuadtreeAtlas atlas = {
        .arena = arena,
        .root = {
            .size = size,
        },
        .size = size,
        .bitmap = sp_arena_push(arena, size.x * size.y),
    };
    return atlas;
}

static u32 align_value_up(u32 value, u32 align) {
    u64 aligned = value + align - 1;
    u64 mod = aligned % align;
    aligned = aligned - mod;
    return aligned;
}

static QuadtreeAtlasNode* quadtree_atlas_insert_helper(SP_Arena* arena, QuadtreeAtlasNode* node, SP_Ivec2 size) {
    if (node == NULL || node->occupied || node->size.x < size.x || node->size.y < size.y) {
        return NULL;
    }

    if (!node->split) {
        if (node->size.x == size.x && node->size.y == size.y) {
            node->occupied = true;
            return node;
        }

        node->children = sp_arena_push_no_zero(arena, 4 * sizeof(QuadtreeAtlasNode));
        node->split = true;

        // Dynamic split
        if (node->size.x / 2 < size.x || node->size.y / 2 < size.y) {
            node->children[0] = (QuadtreeAtlasNode) {
                .size = size,
                .pos = node->pos,
                .occupied = true,
            };

            {
                SP_Ivec2 new_size = node->size;
                new_size.x -= size.x;
                new_size.y = size.y;
                SP_Ivec2 pos = node->pos;
                pos.x += size.x;
                node->children[1] = (QuadtreeAtlasNode) {
                    .size = new_size,
                    .pos = pos,
                };
            }

            {
                SP_Ivec2 new_size = node->size;
                new_size.x = size.x;
                new_size.y -= size.y;
                SP_Ivec2 pos = node->pos;
                pos.y += size.y;
                node->children[2] = (QuadtreeAtlasNode) {
                    .size = new_size,
                    .pos = pos,
                };
            }

            return &node->children[0];
        }

        SP_Ivec2 half_size = sp_iv2_divs(node->size, 2);
        node->children[0] = (QuadtreeAtlasNode) {
            .size = half_size,
            .pos = node->pos,
        };
        node->children[1] = (QuadtreeAtlasNode) {
            .size = half_size,
            .pos = sp_iv2(node->pos.x + half_size.x, node->pos.y),
        };
        node->children[2] = (QuadtreeAtlasNode) {
            .size = half_size,
            .pos = sp_iv2(node->pos.x, node->pos.y + half_size.y),
        };
        node->children[3] = (QuadtreeAtlasNode) {
            .size = half_size,
            .pos = sp_iv2_add(node->pos, half_size),
        };
    }

    for (u8 i = 0; i < 4; i++) {
        QuadtreeAtlasNode* result = quadtree_atlas_insert_helper(arena, &node->children[i], size);
        if (result != NULL) {
            return result;
        }
    }

    return NULL;
}

static QuadtreeAtlasNode* quadtree_atlas_insert(QuadtreeAtlas* atlas, SP_Ivec2 size) {
    size.x = align_value_up(size.x, 4);
    size.y = align_value_up(size.y, 4);
    return quadtree_atlas_insert_helper(atlas->arena, &atlas->root, size);
}

// -- Font provider ------------------------------------------------------------

// Font provider glyph
typedef struct FPGlyph FPGlyph;
struct FPGlyph {
    struct {
        u8* buffer;
        SP_Ivec2 size;
    } bitmap;
    SP_Vec2 size;
    SP_Vec2 offset;
    f32 advance;
};

typedef struct FontProvider FontProvider;
struct FontProvider {
    void* (*init)(SP_Arena* arena, SP_Str ttf_data);
    void (*terminate)(void* internal);
    u32 (*get_glyph_index)(void* internal, u32 codepoint);
    FPGlyph (*get_glyph)(void* internal, SP_Arena* arena, u32 glyph_index, f32 size);
    RNE_FontMetrics (*get_metrics)(void* internal, f32 size);
    i32 (*get_kerning)(void* internal, u32 left_glyph, u32 right_glyph, f32 size);
};

// -- stb_truetype font provider -----------------------------------------------

typedef struct STBTTInternal STBTTInternal;
struct STBTTInternal {
    stbtt_fontinfo info;
};

static void* fp_stbtt_init(SP_Arena* arena, SP_Str ttf_data) {
    STBTTInternal* internal = sp_arena_push_no_zero(arena, sizeof(STBTTInternal));

    const u8* ttf_cstr = (const u8*) sp_str_to_cstr(arena, ttf_data);
    if (!stbtt_InitFont(&internal->info, ttf_cstr, stbtt_GetFontOffsetForIndex(ttf_cstr, 0))) {
        sp_error("STBTT: Init error!");
    }

    return internal;
}

static void fp_stbtt_terminate(void* internal) {
    (void) internal;
}

static u32 fp_stbtt_get_glyph_index(void* internal, u32 codepoint) {
    STBTTInternal* stbtt = internal;
    u32 glyph_index = stbtt_FindGlyphIndex(&stbtt->info, codepoint);
    return glyph_index;
}

static FPGlyph fp_stbtt_get_glyph(void* internal, SP_Arena* arena, u32 glyph_index, f32 size) {
    STBTTInternal* stbtt = internal;
    i32 advance;
    i32 lsb;
    stbtt_GetGlyphHMetrics(&stbtt->info, glyph_index, &advance, &lsb);

    f32 scale = stbtt_ScaleForPixelHeight(&stbtt->info, size);
    SP_Ivec2 i0;
    SP_Ivec2 i1;
    stbtt_GetGlyphBitmapBox(&stbtt->info, glyph_index, scale, scale, &i0.x, &i0.y, &i1.x, &i1.y);
    SP_Ivec2 bitmap_size = sp_iv2_sub(i1, i0);
    SP_Vec2 glyph_size = sp_v2(bitmap_size.x, bitmap_size.y);
    u32 bitmap_area = bitmap_size.x * bitmap_size.y;
    u8* bitmap = sp_arena_push_no_zero(arena, bitmap_area);
    stbtt_MakeGlyphBitmap(&stbtt->info, bitmap, bitmap_size.x, bitmap_size.y, bitmap_size.x, scale, scale, glyph_index);

    FPGlyph glyph = {
        .advance = floorf(advance * scale),
        .bitmap = {
            .size = bitmap_size,
            .buffer = bitmap,
        },
        .size = glyph_size,
        .offset = sp_v2(floorf(lsb * scale), i0.y),
    };
    return glyph;
}

static RNE_FontMetrics fp_stbtt_get_metrics(void* internal, f32 size) {
    STBTTInternal* stbtt = internal;
    f32 scale = stbtt_ScaleForPixelHeight(&stbtt->info, size);
    i32 ascent;
    i32 descent;
    i32 linegap;
    stbtt_GetFontVMetrics(&stbtt->info, &ascent, &descent, &linegap);
    return (RNE_FontMetrics) {
        .ascent = floorf(ascent * scale),
        .descent = floorf(descent * scale),
        .linegap = floorf(linegap * scale),
    };
}

static i32 fp_stbtt_get_kerning(void* internal, u32 left_glyph, u32 right_glyph, f32 size) {
    STBTTInternal* stbtt = internal;
    f32 scale = stbtt_ScaleForPixelHeight(&stbtt->info, size);
    f32 kern = stbtt_GetGlyphKernAdvance(&stbtt->info, left_glyph, right_glyph);
    return floorf(kern * scale);
}

static const FontProvider STBTT_PROVIDER = {
    .init = fp_stbtt_init,
    .terminate = fp_stbtt_terminate,
    .get_glyph_index = fp_stbtt_get_glyph_index,
    .get_glyph = fp_stbtt_get_glyph,
    .get_metrics = fp_stbtt_get_metrics,
    .get_kerning = fp_stbtt_get_kerning,
};

// -- User API -----------------------------------------------------------------

typedef struct RNE_GlyphInternal RNE_GlyphInternal;
struct RNE_GlyphInternal {
    RNE_Glyph user_glyph;
    u8* bitmap;
    SP_Ivec2 bitmap_size;
};

typedef struct RNE_SizedFont RNE_SizedFont;
struct RNE_SizedFont {
    f32 size;
    QuadtreeAtlas atlas_packer;
    RNE_UserData userdata_atlas;
    // Key: u32 (glyph index)
    // Value: GlyphInternal
    SP_HashMap* glyph_map;
};

typedef struct RNE_Font RNE_Font;
struct RNE_Font {
    SP_Arena* arena;
    RNE_FontCallbacks cb;
    SP_Str ttf_data;
    FontProvider provider;

    void* internal;
    // Key: f32 (size)
    // Value: RNE_SizedFont
    SP_HashMap* map;
};

static RNE_SizedFont sized_font_create(RNE_Font* font, f32 size) {
    SP_Ivec2 atlas_size = sp_iv2(256, 256);
    RNE_SizedFont sized = {
        .size = size,
        .atlas_packer = quadtree_atlas_init(font->arena, atlas_size),
        .userdata_atlas = font->cb.create(atlas_size),
        .glyph_map = sp_hm_new(sp_hm_desc_generic(font->arena, 32, u32, RNE_GlyphInternal)),
    };
    return sized;
}

RNE_Handle rne_font_create(SP_Arena* arena, SP_Str ttf_data, RNE_FontCallbacks callbacks) {
    RNE_Font* font = sp_arena_push_no_zero(arena, sizeof(RNE_Font));
    FontProvider provider = STBTT_PROVIDER;
    *font = (RNE_Font) {
        .arena = arena,
        .provider = provider,
        .internal = provider.init(arena, ttf_data),
        .ttf_data = ttf_data,
        .map = sp_hm_new(sp_hm_desc_generic(arena, 32, f32, RNE_SizedFont)),
        .cb = callbacks,
    };

    return (RNE_Handle) {
        .ptr = font,
    };
}

void rne_font_destroy(RNE_Handle* font) {
    RNE_Font* _font = font->ptr;
    for (SP_HashMapIter iter = sp_hm_iter_new(_font->map);
            sp_hm_iter_valid(iter);
            iter = sp_hm_iter_next(iter)) {
        RNE_SizedFont sized = sp_hm_iter_get_value(iter, RNE_SizedFont);
        _font->cb.destroy(sized.userdata_atlas);
    }

    _font->provider.terminate(_font->internal);
}

static RNE_SizedFont* get_sized_font(RNE_Font* font, f32 size) {
    RNE_SizedFont* result = sp_hm_getp(font->map, size);
    if (result == NULL) {
        RNE_SizedFont sized = sized_font_create(font, size);
        sp_hm_insert(font->map, size, sized);
        result = sp_hm_getp(font->map, size);
    }
    return result;
}

static void calculate_uvs(SP_Ivec2 pos, SP_Ivec2 size, SP_Ivec2 atlas_size, SP_Vec2 uvs[2]) {
    SP_Vec2 uv_tl = sp_iv2_to_v2(pos);
    SP_Vec2 uv_br = sp_v2_add(uv_tl, sp_iv2_to_v2(size));

    SP_Vec2 atlas_size_f = sp_iv2_to_v2(atlas_size);
    uv_tl = sp_v2_div(uv_tl, atlas_size_f);
    uv_br = sp_v2_div(uv_br, atlas_size_f);

    uvs[0] = uv_tl;
    uvs[1] = uv_br;
}

static void expand_atlas(RNE_Font* font, RNE_SizedFont* sized) {
    QuadtreeAtlas packer = quadtree_atlas_init(font->arena, sp_iv2_muls(sized->atlas_packer.size, 2));

    SP_Scratch scratch = sp_scratch_begin(&font->arena, 1);
    u8* bitmap = sp_arena_push(scratch.arena, packer.size.x * packer.size.y);

    SP_HashMapIter iter = sp_hm_iter_new(sized->glyph_map);
    while (sp_hm_iter_valid(iter)) {
        RNE_GlyphInternal* glyph = sp_hm_iter_get_valuep(iter);
        QuadtreeAtlasNode* node = quadtree_atlas_insert(&packer, glyph->bitmap_size);
        for (i32 y = 0; y < glyph->bitmap_size.y; y++) {
            u32 atlas_index = node->pos.x + (node->pos.y + y) * packer.size.x;
            u32 glyph_index = y * glyph->bitmap_size.x;
            memcpy(&bitmap[atlas_index], &glyph->bitmap[glyph_index], glyph->bitmap_size.x);
        }

        SP_Vec2 atlas_size = sp_v2(packer.size.x, packer.size.y);
        calculate_uvs(node->pos, node->size, packer.size, glyph->user_glyph.uv);

        iter = sp_hm_iter_next(iter);
    }

    font->cb.resize(sized->userdata_atlas, packer.size, bitmap);
    sp_scratch_end(scratch);

    sized->atlas_packer = packer;
}

SP_Vec2 rne_text_measure(RNE_Handle font, SP_Str text, f32 size) {
    RNE_FontMetrics metrics = rne_font_get_metrics(font, size);
    SP_Vec2 text_size = sp_v2(0.0f, metrics.ascent - metrics.descent);
    for (u32 i = 0; i < text.len; i++) {
        RNE_Glyph glyph = rne_font_get_glyph(font, text.data[i], size);
        text_size.x += glyph.advance;
        if (i < text.len - 1) {
            text_size.x += rne_font_get_kerning(font, text.data[i], text.data[i+1], size);
        }
    }
    return text_size;
}

RNE_Glyph rne_font_get_glyph(RNE_Handle font, u32 codepoint, f32 size) {
    RNE_Font* _font = font.ptr;
    RNE_SizedFont* sized = get_sized_font(_font, size);

    u32 glyph_index = _font->provider.get_glyph_index(_font->internal, codepoint);
    RNE_Glyph* _glyph = sp_hm_getp(sized->glyph_map, glyph_index);
    if (_glyph != NULL) {
        return *_glyph;
    }

    SP_Scratch scratch = sp_scratch_begin(&_font->arena, 1);
    FPGlyph fp_glyph = _font->provider.get_glyph(_font->internal, scratch.arena, glyph_index, size);
    u32 bitmap_size = fp_glyph.bitmap.size.x * fp_glyph.bitmap.size.y;
    u8* bitmap = sp_arena_push(_font->arena, bitmap_size);
    memcpy(bitmap, fp_glyph.bitmap.buffer, bitmap_size);
    sp_scratch_end(scratch);

    QuadtreeAtlasNode* node = quadtree_atlas_insert(&sized->atlas_packer, fp_glyph.bitmap.size);
    // Atlas out of space.
    while (node == NULL) {
        expand_atlas(_font, sized);
        node = quadtree_atlas_insert(&sized->atlas_packer, fp_glyph.bitmap.size);
    }

    _font->cb.update(sized->userdata_atlas, node->pos, fp_glyph.bitmap.size, sized->atlas_packer.size.x, bitmap);

    RNE_GlyphInternal glyph = {
        .user_glyph = {
            .size = fp_glyph.size,
            .offset = fp_glyph.offset,
            .advance = fp_glyph.advance,
        },
        .bitmap_size = fp_glyph.bitmap.size,
        .bitmap = bitmap,
    };
    calculate_uvs(node->pos, sp_v2_to_iv2(fp_glyph.size), sized->atlas_packer.size, glyph.user_glyph.uv);
    sp_hm_insert(sized->glyph_map, glyph_index, glyph);

    return glyph.user_glyph;
}

RNE_Handle rne_font_get_atlas(RNE_Handle font, f32 size) {
    RNE_Font* _font = font.ptr;
    RNE_SizedFont* sized = get_sized_font(_font, size);
    return sized->userdata_atlas;
}

RNE_FontMetrics rne_font_get_metrics(RNE_Handle font, f32 size) {
    RNE_Font* _font = font.ptr;
    return _font->provider.get_metrics(_font->internal, size);
}

f32 rne_font_get_kerning(RNE_Handle font, u32 left_codepoint, u32 right_codepoint, f32 size) {
    RNE_Font* _font = font.ptr;
    u32 left_glyph = _font->provider.get_glyph_index(_font->internal, left_codepoint);
    u32 right_glyph = _font->provider.get_glyph_index(_font->internal, right_codepoint);
    return _font->provider.get_kerning(_font->internal, left_glyph, right_glyph, size);
}
