#include "renderer.h"
#include "font.h"

#include <glad/gl.h>

Renderer renderer_create(SP_Arena* arena) {
    // Vertices
    const u32 MAX_QUAD_COUNT = 4096;
    u32 vert_size = MAX_QUAD_COUNT * 4 * sizeof(Vertex);
    Vertex* vertices = sp_arena_push_no_zero(arena, vert_size);

    Renderer renderer = {
        .max_quads = MAX_QUAD_COUNT,
        .vertices = vertices,
    };

    // Vertex buffer
    glGenBuffers(1, &renderer.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer.vbo);
    glBufferData(GL_ARRAY_BUFFER, vert_size, NULL, GL_DYNAMIC_DRAW);

    // Indices
    SP_Temp temp = sp_temp_begin(arena);
    u32 indices_size = MAX_QUAD_COUNT * 6 * sizeof(u32);
    u32* indices = sp_arena_push_no_zero(temp.arena, indices_size);
    u32 j = 0;
    for (u32 i = 0; i < MAX_QUAD_COUNT * 6; i += 6) {
        indices[i + 5] = j + 1;
        indices[i + 0] = j + 0;
        indices[i + 1] = j + 1;
        indices[i + 2] = j + 2;
        indices[i + 3] = j + 2;
        indices[i + 4] = j + 3;
        j += 4;
    }
    // Index buffer
    glGenBuffers(1, &renderer.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_size, indices, GL_STATIC_DRAW);
    sp_temp_end(temp);

    // Vertex array
    glGenVertexArrays(1, &renderer.vao);
    glBindVertexArray(renderer.vao);

    // Vertex layout
    // Position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*) sp_offset(Vertex, pos));
    glEnableVertexAttribArray(0);
    // UV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*) sp_offset(Vertex, uv));
    glEnableVertexAttribArray(1);
    // Color
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*) sp_offset(Vertex, color));
    glEnableVertexAttribArray(2);
    // Texture Index
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(Vertex), (const void*) sp_offset(Vertex, texture_index));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Shader
    const SP_Str vertex_source = sp_str_lit(
            "#version 460 core\n"
            "layout (location = 0) in vec2 aPos;\n"
            "layout (location = 1) in vec2 aUV;\n"
            "layout (location = 2) in vec4 aColor;\n"
            "layout (location = 3) in uint aTextureIndex;\n"
            "out vec4 color;\n"
            "out vec2 uv;\n"
            "out uint textureIndex;\n"
            "uniform mat4 projection;\n"
            "void main() {\n"
            "    color = aColor;\n"
            "    uv = aUV;\n"
            "    textureIndex = aTextureIndex;\n"
            "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
            "}\n"
        );
    const SP_Str fragment_source = sp_str_lit(
            "#version 460 core\n"
            "layout (location = 0) out vec4 fragColor;\n"
            "in vec4 color;\n"
            "in vec2 uv;\n"
            "in flat uint textureIndex;\n"
            "uniform sampler2D textures[32];\n"
            "void main() {\n"
            "    fragColor = texture(textures[textureIndex], uv) * color;\n"
            "}\n"
        );

    i32 success = 0;
    char info_log[512] = {0};

    u32 v_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v_shader, 1, (const char* const*) &vertex_source.data, (const int*) &vertex_source.len);
    glCompileShader(v_shader);
    glGetShaderiv(v_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(v_shader, sizeof(info_log), NULL, info_log);
        sp_error("Vertex shader compilation error: %s", info_log);
    }

    u32 f_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f_shader, 1, (const char* const*) &fragment_source.data, (const int*) &fragment_source.len);
    glCompileShader(f_shader);
    glGetShaderiv(f_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(f_shader, sizeof(info_log), NULL, info_log);
        sp_error("Fragment shader compilation error: %s\n", info_log);
    }

    renderer.shader = glCreateProgram();
    glAttachShader(renderer.shader, v_shader);
    glAttachShader(renderer.shader, f_shader);
    glLinkProgram(renderer.shader);
    glGetProgramiv(renderer.shader, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(renderer.shader, 512, NULL, info_log);
        sp_error("Shader linking error: %s\n", info_log);
    }

    glDeleteShader(v_shader);
    glDeleteShader(f_shader);

    // Bind samplers to correct unit
    i32 samplers[32];
    for (u32 i = 0; i < 32; i++) {
        samplers[i] = i;
    }
    glUseProgram(renderer.shader);
    u32 loc = glGetUniformLocation(renderer.shader, "textures");
    glUniform1iv(loc, sp_arrlen(samplers), samplers);

    // White texture
    glGenTextures(1, &renderer.textures[0]);
    glBindTexture(GL_TEXTURE_2D, renderer.textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, (u32[]) {0xffffffff});
    renderer.curr_texture = 1;

    return renderer;
}

void renderer_destroy(Renderer* renderer) {
    glDeleteVertexArrays(1, &renderer->vao);
    glDeleteBuffers(1, &renderer->vbo);
    glDeleteBuffers(1, &renderer->ibo);
    glDeleteProgram(renderer->shader);
    glDeleteTextures(1, &renderer->textures[0]);

    *renderer = (Renderer) {0};
}

void renderer_begin(Renderer* renderer, SP_Ivec2 screen_size) {
    // Projection
    f32 zoom = screen_size.y / 2.0f;
    f32 aspect = (f32) screen_size.x / screen_size.y;
    renderer->projection = sp_m4_ortho_projection(-aspect * zoom, aspect * zoom, zoom, -zoom, 1.0f, -1.0f);
    // Translate to edge of screen.
    renderer->projection.d.x -= 1.0f;
    renderer->projection.d.y += 1.0f;

    // Reset state
    renderer->curr_quad = 0;
    renderer->curr_texture = 1;
}

void renderer_end(Renderer* renderer) {
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->curr_quad * 4 * sizeof(Vertex), renderer->vertices);

    for (u32 i = 0; i < renderer->curr_texture; i++) {
        glBindTextureUnit(i, renderer->textures[i]);
    }

    glUseProgram(renderer->shader);
    u32 loc = glGetUniformLocation(renderer->shader, "projection");
    glUniformMatrix4fv(loc, 1, false, &renderer->projection.a.x);

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ibo);
    glDrawElements(GL_TRIANGLES, renderer->curr_quad * 6, GL_UNSIGNED_INT, 0);
}

void renderer_draw(Renderer* renderer, RenderBox box) {
    u32 texture_index = 0;
    b8 found_texture = false;
    if (box.texture == 0) {
        found_texture = true;
    } else {
        for (u32 i = 0; i < renderer->curr_texture; i++) {
            if (renderer->textures[i] == box.texture) {
                found_texture = true;
                texture_index = i;
                break;
            }
        }
        if (!found_texture) {
            texture_index = renderer->curr_texture;
            renderer->textures[renderer->curr_texture] = box.texture;
            renderer->curr_texture++;
        }
    }

    const SP_Vec2 vert_pos[4] = {
        sp_v2(0.0f,  0.0f),
        sp_v2(1.0f,  0.0f),
        sp_v2(0.0f, -1.0f),
        sp_v2(1.0f, -1.0f),
    };

    if (!box.has_uv) {
        box.uv[0] = sp_v2s(0.0f);
        box.uv[1] = sp_v2s(1.0f);
    }
    f32 l = box.uv[0].x;
    f32 r = box.uv[1].x;
    f32 t = box.uv[0].y;
    f32 b = box.uv[1].y;
    const SP_Vec2 vert_uv[4] = {
        sp_v2(l, t),
        sp_v2(r, t),
        sp_v2(l, b),
        sp_v2(r, b),
    };

    box.pos.y = -box.pos.y;
    for (u32 i = 0; i < 4; i++) {
        Vertex* vert = &renderer->vertices[renderer->curr_quad * 4 + i];
        vert->pos = vert_pos[i];
        vert->pos = sp_v2_mul(vert->pos, box.size);
        vert->pos = sp_v2_add(vert->pos, box.pos);
        vert->color = box.color;
        vert->uv = vert_uv[i];
        vert->texture_index = texture_index;
    }

    renderer->curr_quad++;
}

void renderer_draw_text(Renderer* renderer, SP_Vec2 pos, SP_Str text, Font* font, SP_Vec4 color) {
    FontMetrics metrics = font_get_metrics(font);
    SP_Vec2 gpos = pos;
    gpos.y += metrics.ascent;
    u32 atlas = font_get_atlas(font);
    for (u32 i = 0; i < text.len; i++) {
        Glyph glyph = font_get_glyph(font, text.data[i]);
        SP_Vec2 non_snapped = sp_v2_add(gpos, glyph.offset);
        SP_Vec2 snapped = sp_v2(
                    floorf(non_snapped.x),
                    floorf(non_snapped.y)
                );
        renderer_draw(renderer, (RenderBox) {
                .pos = snapped,
                .size = glyph.size,
                .color = color,
                .texture = atlas,
                .has_uv = true,
                .uv = {glyph.uv[0], glyph.uv[1]},
            });
        gpos.x += glyph.advance;
        if (i < text.len - 1) {
            gpos.x += font_get_kerning(font, text.data[i], text.data[i+1]);
        }
    }
}
