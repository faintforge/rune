#include "font.h"

#include <glad/gl.h>
#include <stb_truetype.h>

#include <stdio.h>
#include <string.h>

SP_Str read_file(SP_Arena* arena, SP_Str filename) {
    const char* cstr_filename = sp_str_to_cstr(arena, filename);
    FILE *fp = fopen(cstr_filename, "rb");
    // Pop off the cstr_filename from the arena since it's no longer needed.
    sp_arena_pop(arena, filename.len + 1);
    if (fp == NULL) {
        sp_error("Failed to open file '%.*s'.", filename.len, filename.data);
        return (SP_Str) {0};
    }

    fseek(fp, 0, SEEK_END);
    u32 len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    u8* content = sp_arena_push(arena, len);
    fread(content, sizeof(u8), len, fp);

    return sp_str(content, len);
}

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

QuadtreeAtlas quadtree_atlas_init(SP_Arena* arena, SP_Ivec2 size) {
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

u32 align_value_up(u32 value, u32 align) {
    u64 aligned = value + align - 1;
    u64 mod = aligned % align;
    aligned = aligned - mod;
    return aligned;
}

QuadtreeAtlasNode* quadtree_atlas_insert_helper(SP_Arena* arena, QuadtreeAtlasNode* node, SP_Ivec2 size) {
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

QuadtreeAtlasNode* quadtree_atlas_insert(QuadtreeAtlas* atlas, SP_Ivec2 size) {
    size.x = align_value_up(size.x, 4);
    size.y = align_value_up(size.y, 4);
    return quadtree_atlas_insert_helper(atlas->arena, &atlas->root, size);
}

// void quadtree_atlas_debug_draw_helper(SP_Ivec2 atlas_size, QuadtreeAtlasNode* node, Quad quad, Camera cam) {
//     if (node == NULL) {
//         return;
//     }
//
//     SP_Vec2 size = sp_v2(
//             (f32) node->size.x / (f32) atlas_size.x,
//             (f32) node->size.y / (f32) atlas_size.y
//         );
//     size = sp_v2_mul(size, quad.size);
//
//     SP_Vec2 pos = sp_v2(
//             (f32) node->pos.x / (f32) atlas_size.x,
//             -(f32) node->pos.y / (f32) atlas_size.y
//         );
//     pos = sp_v2_mul(pos, quad.size);
//     pos = sp_v2_add(pos, quad.pos);
//
//     debug_draw_quad_outline((Quad) {
//             .pos = pos,
//             .size = size,
//             .color = color_rgb_hex(0x808080),
//             .pivot = sp_v2(-0.5f, 0.5f),
//         }, cam);
//
//     if (node->split) {
//         for (u8 i = 0; i < 4; i++) {
//             quadtree_atlas_debug_draw_helper(atlas_size, &node->children[i], quad, cam);
//         }
//     }
// }

// void quadtree_atlas_debug_draw(QuadtreeAtlas atlas, Quad quad, Camera cam) {
//     debug_draw_quad((Quad) {
//             .pos = quad.pos,
//             .size = quad.size,
//             .pivot = sp_v2(-0.5f, 0.5f),
//             .color = quad.color,
//             .texture = quad.texture,
//         }, cam);
//     quadtree_atlas_debug_draw_helper(atlas.root.size, &atlas.root, quad, cam);
// }

// -- Font providers -----------------------------------------------------------

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
    FPGlyph (*get_glyph)(void* internal, SP_Arena* arena, u32 glyph_index, u32 size);
    FontMetrics (*get_metrics)(void* internal, u32 size);
    i32 (*get_kerning)(void* internal, u32 left_glyph, u32 right_glyph, u32 size);
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

static FPGlyph fp_stbtt_get_glyph(void* internal, SP_Arena* arena, u32 glyph_index, u32 size) {
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
        // .offset = sp_v2(i0.x - floorf(lsb * scale), i0.y),
        // .offset = sp_v2(i0.x, i0.y),
        .offset = sp_v2(floorf(lsb * scale), i0.y),
    };
    return glyph;
}

static FontMetrics fp_stbtt_get_metrics(void* internal, u32 size) {
    STBTTInternal* stbtt = internal;
    f32 scale = stbtt_ScaleForPixelHeight(&stbtt->info, size);
    i32 ascent;
    i32 descent;
    i32 linegap;
    stbtt_GetFontVMetrics(&stbtt->info, &ascent, &descent, &linegap);
    return (FontMetrics) {
        .ascent = floorf(ascent * scale),
        .descent = floorf(descent * scale),
        .linegap = floorf(linegap * scale),
    };
}

static i32 fp_stbtt_get_kerning(void* internal, u32 left_glyph, u32 right_glyph, u32 size) {
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

// -- Forward facing API -------------------------------------------------------

typedef struct GlyphInternal GlyphInternal;
struct GlyphInternal {
    Glyph user_glyph;
    u8* bitmap;
    SP_Ivec2 bitmap_size;
};

typedef struct SizedFont SizedFont;
struct SizedFont {
    u32 size;
    QuadtreeAtlas atlas_packer;
    u32 atlas_texture;
    FontMetrics metrics;
    // Key: u32 (glyph index)
    // Value: GlyphInternal
    SP_HashMap* glyph_map;
};

struct Font {
    SP_Arena* arena;
    FontProvider provider;
    SP_Str ttf_data;

    void* internal;
    u32 curr_size;
    SP_HashMap* map;
};

SizedFont sized_font_create(Font* font, u32 size) {
    SP_Ivec2 atlas_size = sp_iv2(256, 256);
    // Provide a buffer with zeros so the texture is properly cleared.
    SP_Scratch scratch = sp_scratch_begin(NULL, 0);
    u8* zero_buffer = sp_arena_push(scratch.arena, atlas_size.x * atlas_size.y);

    u32 texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Swizzle components
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_R8,
            atlas_size.x,
            atlas_size.y,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            zero_buffer);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    SizedFont sized = {
        .size = size,
        .atlas_packer = quadtree_atlas_init(font->arena, atlas_size),
        .atlas_texture = texture,
        .metrics = font->provider.get_metrics(font->internal, size),
        .glyph_map = sp_hm_new(sp_hm_desc_generic(font->arena, 32, u32, GlyphInternal)),
    };
    sp_scratch_end(scratch);
    return sized;
}

Font* font_create(SP_Arena* arena, SP_Str filename) {
    Font* font = sp_arena_push_no_zero(arena, sizeof(Font));
    SP_Str ttf_data = read_file(arena, filename);
    // Push a zero onto the arena so the 'ttf_data' string works as a cstr as
    // well.
    sp_arena_push(arena, 1);
    // FontProvider provider = FT2_PROVIDER;
    FontProvider provider = STBTT_PROVIDER;
    *font = (Font) {
        .arena = arena,
        .provider = provider,
        .internal = provider.init(arena, ttf_data),
        .ttf_data = ttf_data,
        .map = sp_hm_new(sp_hm_desc_generic(arena, 32, u32, SizedFont)),
    };
    return font;
}

void font_destroy(Font* font) {
    font->provider.terminate(font->internal);
}

void font_set_size(Font* font, u32 size) {
    font->curr_size = size;
    if (!sp_hm_has(font->map, size)) {
        SizedFont sized = sized_font_create(font, size);
        sp_hm_insert(font->map, size, sized);
    }
}

static void calculate_uvs(SP_Ivec2 pos, SP_Ivec2 size, SP_Ivec2 atlas_size, SP_Vec2 uvs[2]) {
    SP_Vec2 uv_tl = sp_iv2_to_v2(pos);
    SP_Vec2 uv_br = sp_v2_add(uv_tl, sp_iv2_to_v2(size));

    // Offset by 0.5 to have the UV be in the middle of a texel so we don't
    // have bleeding when filtering.
    // uv_tl = sp_v2_adds(uv_tl, 0.5f);
    // uv_br = sp_v2_subs(uv_br, 0.5f);

    SP_Vec2 atlas_size_f = sp_iv2_to_v2(atlas_size);
    uv_tl = sp_v2_div(uv_tl, atlas_size_f);
    uv_br = sp_v2_div(uv_br, atlas_size_f);

    uvs[0] = uv_tl;
    uvs[1] = uv_br;
}

static void expand_atlas(Font* font, SizedFont* sized) {
    QuadtreeAtlas packer = quadtree_atlas_init(font->arena, sp_iv2_muls(sized->atlas_packer.size, 2));

    SP_Scratch scratch = sp_scratch_begin(NULL, 0);
    u8* bitmap = sp_arena_push(scratch.arena, packer.size.x * packer.size.y);

    SP_HashMapIter iter = sp_hm_iter_new(sized->glyph_map);
    while (sp_hm_iter_valid(iter)) {
        GlyphInternal* glyph = sp_hm_iter_get_valuep(iter);
        QuadtreeAtlasNode* node = quadtree_atlas_insert(&packer, glyph->bitmap_size);
        for (i32 y = 0; y < glyph->bitmap_size.y; y++) {
            u32 atlas_index = node->pos.x + (node->pos.y + y) * packer.size.x;
            u32 glyph_index = y * glyph->bitmap_size.x;
            memcpy(&bitmap[atlas_index], &glyph->bitmap[glyph_index], glyph->bitmap_size.x);
        }

        SP_Vec2 atlas_size = sp_v2(packer.size.x, packer.size.y);
        // SP_Vec2 uv_tl = sp_v2(node->pos.x, node->pos.y);
        // SP_Vec2 uv_br = uv_tl;
        // Offset by 0.5 to have the UV be in the middle of a texel so we don't
        // have bleeding when filtering.
        // uv_tl = sp_v2_subs(uv_tl, 0.5f);
        // uv_br = sp_v2_adds(uv_br, 0.5f);
        // uv_tl = sp_v2_div(uv_tl, atlas_size);
        // uv_br = sp_v2_div(uv_br, atlas_size);

        calculate_uvs(node->pos, node->size, packer.size, glyph->user_glyph.uv);
        // glyph->user_glyph.uv[0] = uv_tl;
        // glyph->user_glyph.uv[1] = uv_br;

        iter = sp_hm_iter_next(iter);
    }

    // Resize texture.
    glBindTexture(GL_TEXTURE_2D, sized->atlas_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_R8,
            packer.size.x,
            packer.size.y,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            bitmap);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    sp_scratch_end(scratch);

    sized->atlas_packer = packer;
}

Glyph font_get_glyph(Font* font, u32 codepoint) {
    SizedFont* sized = sp_hm_getp(font->map, font->curr_size);
    if (sized == NULL) {
        sp_error("Font of size %u hasn't been created.", font->curr_size);
        return (Glyph) {0};
    }

    u32 glyph_index = font->provider.get_glyph_index(font->internal, codepoint);
    Glyph* _glyph = sp_hm_getp(sized->glyph_map, glyph_index);
    if (_glyph != NULL) {
        return *_glyph;
    }

    SP_Scratch scratch = sp_scratch_begin(&font->arena, 1);
    FPGlyph fp_glyph = font->provider.get_glyph(font->internal, scratch.arena, glyph_index, sized->size);
    u32 bitmap_size = fp_glyph.bitmap.size.x * fp_glyph.bitmap.size.y;
    u8* bitmap = sp_arena_push(font->arena, bitmap_size);
    memcpy(bitmap, fp_glyph.bitmap.buffer, bitmap_size);
    sp_scratch_end(scratch);

    QuadtreeAtlasNode* node = quadtree_atlas_insert(&sized->atlas_packer, fp_glyph.bitmap.size);
    // Atlas out of space.
    while (node == NULL) {
        expand_atlas(font, sized);
        node = quadtree_atlas_insert(&sized->atlas_packer, fp_glyph.bitmap.size);
    }

    // Update subregion of texture.
    glBindTexture(GL_TEXTURE_2D, sized->atlas_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D,
            0,
            node->pos.x,
            node->pos.y,
            fp_glyph.size.x,
            fp_glyph.size.y,
            GL_RED,
            GL_UNSIGNED_BYTE,
            bitmap);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // SP_Vec2 atlas_size = sp_v2(sized->atlas_packer.size.x, sized->atlas_packer.size.y);
    // SP_Vec2 uv_tl = sp_v2_div(sp_v2(node->pos.x, node->pos.y), atlas_size);
    // SP_Vec2 uv_br = sp_v2_div(sp_v2(node->pos.x + fp_glyph.size.x, node->pos.y + fp_glyph.size.y), atlas_size);
    GlyphInternal glyph = {
        .user_glyph = {
            .size = fp_glyph.size,
            // .uv = {uv_tl, uv_br},
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

u32 font_get_atlas(const Font* font) {
    SizedFont* sized = sp_hm_getp(font->map, font->curr_size);
    if (sized == NULL) {
        sp_error("Font of size %u hasn't been created.", font->curr_size);
        return -1;
    }

    return sized->atlas_texture;
}

FontMetrics font_get_metrics(const Font* font) {
    SizedFont* sized = sp_hm_getp(font->map, font->curr_size);
    if (sized == NULL) {
        sp_error("Font of size %u hasn't been created.", font->curr_size);
        return (FontMetrics) {0};
    }

    return sized->metrics;
}

f32 font_get_kerning(const Font* font, u32 left_codepoint, u32 right_codepoint) {
    u32 left_glyph = font->provider.get_glyph_index(font->internal, left_codepoint);
    u32 right_glyph = font->provider.get_glyph_index(font->internal, right_codepoint);
    return font->provider.get_kerning(font->internal, left_glyph, right_glyph, font->curr_size);
}

// void debug_font_atlas(const Font* font, Quad quad, Camera cam) {
//     SizedFont* sized = sp_hm_getp(font->map, font->curr_size);
//     if (sized == NULL) {
//         sp_error("Font of size %u hasn't been created.", font->curr_size);
//         return;
//     }
//
//     quadtree_atlas_debug_draw(sized->atlas_packer, quad, cam);
// }

SP_Vec2 font_measure_string(Font* font, SP_Str str) {
    FontMetrics metrics = font_get_metrics(font);
    SP_Vec2 size = sp_v2(0.0f, metrics.ascent - metrics.descent);

    for (u32 i = 0; i < str.len; i++) {
        Glyph glyph = font_get_glyph(font, str.data[i]);
        size.x += glyph.advance;
        if (i < str.len - 1) {
            size.x += font_get_kerning(font, str.data[i], str.data[i+1]);
        }
    }

    return size;
}
