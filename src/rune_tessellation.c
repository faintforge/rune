#include "rune/rune_tessellation.h"
#include "spire.h"

#define PI 3.14159265358979323846

typedef struct Path Path;
struct Path {
    RNE_Vertex* points;
    u32 point_i;
};

static void push_point(Path* path, RNE_Vertex point) {
    path->points[path->point_i] = point;
    path->point_i++;
}

static void push_line(Path* path, RNE_DrawLine line) {
    push_point(path, (RNE_Vertex) {
            .pos = line.a,
            .color = line.color,
            .uv = sp_v2s(0.0f),
        });
    push_point(path, (RNE_Vertex) {
            .pos = line.b,
            .color = line.color,
            .uv = sp_v2s(1.0f),
        });
}

static void push_arc(Path* path, RNE_DrawArc arc) {
    arc.start_angle = -arc.start_angle;
    arc.end_angle = -arc.end_angle;

    // https://en.wikipedia.org/wiki/List_of_trigonometric_identities#Angle_sum_and_difference_identities
    // sin(a + b) = sin(a)*cos(b) + cos(a)*sin(b)
    // cos(a + b) = cos(a)*cos(b) - sin(a)*sin(b)
    f32 sina = sinf(arc.start_angle);
    f32 cosa = cosf(arc.start_angle);
    f32 b = (arc.end_angle - arc.start_angle) / arc.segments;
    f32 sinb = sinf(b);
    f32 cosb = cosf(b);

    f32 current_sin = sina;
    f32 current_cos = cosa;

    for (u32 i = 0; i <= arc.segments; i++) {
        push_point(path, (RNE_Vertex) {
                .pos = sp_v2_add(arc.pos, sp_v2_muls(sp_v2(current_cos, current_sin), arc.radius)),
                .uv = sp_v2(current_cos, current_sin),
                .color = arc.color,
            });

        f32 new_sin = current_sin * cosb + current_cos * sinb;
        f32 new_cos = current_cos * cosb - current_sin * sinb;
        current_sin = new_sin;
        current_cos = new_cos;
    }
}

static void push_circle(Path* path, RNE_DrawCircle circle) {
    push_arc(path, (RNE_DrawArc) {
            .pos = circle.pos,
            .radius = circle.radius,
            .start_angle = 0.0f,
            .end_angle = PI * 2.0f,
            .color = circle.color,
            .segments = circle.segments,
        });
}

static void push_rect(Path* path, RNE_DrawRect rect) {
    f32 min_side = sp_min(rect.size.x, rect.size.y) / 2.0f;
    for (u8 i = 0; i < sp_arrlen(rect.corner_radius.elements); i++) {
        rect.corner_radius.elements[i] = sp_clamp(rect.corner_radius.elements[i], 0.0f, min_side);
    }

    // Top left
    if (rect.corner_radius.x == 0.0f) {
        push_point(path, (RNE_Vertex) {
                .pos = rect.pos,
                .color = rect.color,
            });
    } else {
        push_arc(path, (RNE_DrawArc) {
                .pos = sp_v2(rect.pos.x + rect.corner_radius.x, rect.pos.y + rect.corner_radius.x),
                .radius = rect.corner_radius.x,
                .start_angle = PI / 2.0f,
                .end_angle = PI,
                .color = rect.color,
                .segments = rect.corner_segments,
            });
    }

    // Bottom left
    if (rect.corner_radius.z == 0.0f) {
        push_point(path, (RNE_Vertex) {
                .pos = sp_v2(rect.pos.x, rect.pos.y + rect.size.y),
                .color = rect.color,
            });
    } else {
        push_arc(path, (RNE_DrawArc) {
                .pos = sp_v2(rect.pos.x + rect.corner_radius.z, rect.pos.y + rect.size.y - rect.corner_radius.z),
                .radius = rect.corner_radius.z,
                .start_angle = PI,
                .end_angle = 3.0f * PI / 2.0f,
                .color = rect.color,
                .segments = rect.corner_segments,
                });
    }

    // Bottom right
    if (rect.corner_radius.w == 0.0f) {
        push_point(path, (RNE_Vertex) {
                .pos = sp_v2(rect.pos.x + rect.size.x, rect.pos.y + rect.size.y),
                .color = rect.color,
            });
    } else {
        push_arc(path, (RNE_DrawArc) {
                .pos = sp_v2(rect.pos.x + rect.size.x - rect.corner_radius.w, rect.pos.y + rect.size.y - rect.corner_radius.w),
                .radius = rect.corner_radius.w,
                .start_angle = -PI / 2.0f,
                .end_angle = 0.0f,
                .color = rect.color,
                .segments = rect.corner_segments,
                });
    }

    // Top right
    if (rect.corner_radius.y <= 0.0f) {
        push_point(path, (RNE_Vertex) {
                .pos = sp_v2(rect.pos.x + rect.size.x, rect.pos.y),
                .color = rect.color,
            });
    } else {
        push_arc(path, (RNE_DrawArc) {
                .pos = sp_v2(rect.pos.x + rect.size.x - rect.corner_radius.y, rect.pos.y + rect.corner_radius.y),
                .radius = rect.corner_radius.y,
                .start_angle = 0.0f,
                .end_angle = PI / 2.0f,
                .color = rect.color,
                .segments = rect.corner_segments,
            });
    }
}

static void push_image(Path* path, RNE_DrawImage rect) {
    f32 uv_left   = rect.uv[0].x;
    f32 uv_right  = rect.uv[1].x;
    f32 uv_top    = rect.uv[0].y;
    f32 uv_bottom = rect.uv[1].y;

    // Top left
    push_point(path, (RNE_Vertex) {
            .pos = sp_v2(rect.pos.x, rect.pos.y),
            .color = rect.color,
            .uv = sp_v2(uv_left, uv_top),
        });
    // Bottom left
    push_point(path, (RNE_Vertex) {
            .pos = sp_v2(rect.pos.x, rect.pos.y + rect.size.y),
            .color = rect.color,
            .uv = sp_v2(uv_left, uv_bottom),
        });
    // Bottom right
    push_point(path, (RNE_Vertex) {
            .pos = sp_v2(rect.pos.x + rect.size.x, rect.pos.y + rect.size.y),
            .color = rect.color,
            .uv = sp_v2(uv_right, uv_bottom),
        });
    // Top right
    push_point(path, (RNE_Vertex) {
            .pos = sp_v2(rect.pos.x + rect.size.x, rect.pos.y),
            .color = rect.color,
            .uv = sp_v2(uv_right, uv_top),
        });
}

typedef struct FattenConfig FattenConfig;
struct FattenConfig {
    RNE_Vertex* vertex_buffer;
    u32 vertex_capacity;
    u32 vertex_end;

    u16* index_buffer;
    u32 index_capacity;
    u32 index_end;

    u32 texture_index;
};

typedef struct FattenResult FattenResult;
struct FattenResult {
    u32 vertex_count;
    u32 index_count;
    b8 out_of_memory;
};

static FattenResult path_fill_convex(const Path* path, FattenConfig config) {
    sp_assert(path != NULL, "Path can't be NULL.");
    sp_assert(config.vertex_buffer != NULL, "Vertex buffer can't be NULL.");
    sp_assert(config.index_buffer != NULL, "Index buffer can't be NULL.");

    if (path->point_i < 3) {
        return (FattenResult) {0};
    }

    // OOM
    u32 needed_vertices = path->point_i;
    u32 needed_indices = (path->point_i - 2) * 3;
    sp_assert(needed_vertices <= config.vertex_capacity, "Vertex buffer too small for object!");
    sp_assert(needed_indices <= config.index_capacity, "Index buffer too small for object!");
    if (config.vertex_capacity - config.vertex_end < needed_vertices ||
        config.index_capacity - config.index_end < needed_indices) {
        return (FattenResult) {
            .out_of_memory = true,
        };
    }

    u32 start_offset = config.vertex_end;
    u32 point_count = path->point_i;
    for (u32 i = 0; i < point_count; i++) {
        config.vertex_buffer[start_offset + i] = path->points[i];
        config.vertex_buffer[start_offset + i].texture_index = config.texture_index;
    }

    u32 index_i = config.index_end;
    for (u32 i = 1; i < point_count - 1; i++) {
        config.index_buffer[index_i + 0] = start_offset;
        config.index_buffer[index_i + 1] = start_offset + i;
        config.index_buffer[index_i + 2] = start_offset + i + 1;
        index_i += 3;
    }

    return (FattenResult) {
        .vertex_count = needed_vertices,
        .index_count = needed_indices,
    };
}

static FattenResult path_stroke(const Path* path, FattenConfig config, f32 thickness, b8 closed) {
    sp_assert(path != NULL, "Path can't be NULL.");
    sp_assert(config.vertex_buffer != NULL, "Vertex buffer can't be NULL.");
    sp_assert(config.index_buffer != NULL, "Index buffer can't be NULL.");

    // TODO: UVs

    if (path->point_i < 2 || (path->point_i < 3 && closed)) {
        return (FattenResult) {0};
    }

    // OOM
    u32 needed_vertices = path->point_i * 2;
    u32 needed_indices = closed ? path->point_i * 6 : (path->point_i - 1) * 6;
    sp_assert(needed_vertices <= config.vertex_capacity, "Vertex buffer too small for object!");
    sp_assert(needed_indices <= config.index_capacity, "Index buffer too small for object!");
    if (config.vertex_capacity - config.vertex_end < needed_vertices ||
        config.index_capacity - config.index_end < needed_indices) {
        return (FattenResult) {
            .out_of_memory = true,
        };
    }

    u32 point_count = path->point_i;
    u32 start_offset = config.vertex_end;
    for (u32 i = 0; i < point_count; i++) {
        SP_Vec2 offset;
        SP_Vec2 curr = path->points[i].pos;

        if (!closed && (i == 0 || i + 1 == point_count)) {
            SP_Vec2 normal;
            if (i == 0) {
                SP_Vec2 next = path->points[i + 1].pos;
                normal = sp_v2_sub(next, curr);
            } else {
                SP_Vec2 prev = path->points[i - 1].pos;
                normal = sp_v2_sub(curr, prev);
            }
            normal = sp_v2_normalized(normal);
            normal = sp_v2(-normal.y, normal.x);

            offset = sp_v2_muls(normal, thickness / 2.0f);
        } else {
            u32 i_prev = (point_count + i - 1) % point_count;
            u32 i_next = (i + 1) % point_count;

            SP_Vec2 prev = path->points[i_prev].pos;
            SP_Vec2 next = path->points[i_next].pos;

            // Calculate edge normals
            SP_Vec2 norm1 = sp_v2_sub(curr, prev);
            SP_Vec2 norm2 = sp_v2_sub(next, curr);
            norm1 = sp_v2_normalized(norm1);
            norm2 = sp_v2_normalized(norm2);
            // Rotate 90-degrees to get normal
            norm1 = sp_v2(-norm1.y, norm1.x);
            norm2 = sp_v2(-norm2.y, norm2.x);

            // Honestly, I have no clue what's happening here.
            // It's taken from the Nuklear function
            // 'nk_draw_list_stroke_poly_line'.
            SP_Vec2 avg = sp_v2_divs(sp_v2_add(norm1, norm2), 2.0f);
            if (sp_v2_magnitude_squared(avg) > 0.000001f) {
                f32 scale = 1.0f / sp_v2_magnitude_squared(avg);
                scale = sp_min(scale, 100.0f);
                scale *= thickness / 2.0f;
                avg = sp_v2_muls(avg, scale);
            }
            offset = avg;
        }

        config.vertex_buffer[start_offset + i * 2 + 0] = (RNE_Vertex) {
            .pos = curr,
            .color = path->points[i].color,
        };
        config.vertex_buffer[start_offset + i * 2 + 1] = (RNE_Vertex) {
            .pos = sp_v2_sub(curr, sp_v2_muls(offset, 2.0f)),
            .color = path->points[i].color,
        };
    }

    u32 edge_count = point_count;
    if (!closed) {
        edge_count--;
    }

    u32 index_i = config.index_end;
    for (u32 i = 0; i < edge_count; i++) {
        u32 i_next = (i + 1) % point_count;

        u32 v0 = start_offset + i * 2;
        u32 v1 = start_offset + i * 2 + 1;
        u32 v2 = start_offset + i_next * 2;
        u32 v3 = start_offset + i_next * 2 + 1;

        config.index_buffer[index_i + 0] = v0;
        config.index_buffer[index_i + 1] = v1;
        config.index_buffer[index_i + 2] = v2;

        config.index_buffer[index_i + 3] = v2;
        config.index_buffer[index_i + 4] = v3;
        config.index_buffer[index_i + 5] = v1;

        index_i += 6;
    }

    return (FattenResult) {
        .vertex_count = needed_vertices,
        .index_count = needed_indices,
    };
}

// Resets path after use.
static FattenResult path_fatten(Path* path, FattenConfig config, const RNE_DrawCmd* cmd) {
    FattenResult result = {0};
    if (cmd->filled) {
        result = path_fill_convex(path, config);
    } else {
        result = path_stroke(path, config, cmd->thickness, cmd->closed);
    }
    path->point_i = 0;
    return result;
}

static void push_render_cmd(SP_Arena* arena,
        RNE_RenderCmd** first,
        RNE_RenderCmd** last,
        u32 index_end,
        u32* index_count,
        RNE_DrawScissor scissor) {
    if (*index_count <= 0) {
        return;
    }

    RNE_RenderCmd* render_cmd = sp_arena_push_no_zero(arena, sizeof(RNE_RenderCmd));
    *render_cmd = (RNE_RenderCmd) {
        .start_offset_bytes = (index_end - *index_count) * sizeof(u16),
        .index_count = *index_count,
        .scissor = scissor,
    };
    sp_sll_queue_push(*first, *last, render_cmd);
    *index_count = 0;
}

// Returns index of wanted texture within buffer.
// If buffer is out of space, returns -1.
static u32 find_texture(RNE_Handle* buffer, u32 capacity, u32* count, RNE_Handle wanted) {
    for (u32 i = 0; i < *count; i++) {
        if (buffer[i].ptr == wanted.ptr) {
            return i;
        }
    }

    // Too many textures.
    if (*count == capacity) {
        return -1;
    }

    // Texture not in buffer
    buffer[*count] = wanted;
    (*count)++;
    return *count - 1;
}

static void pre_process_buffer(RNE_DrawCmdBuffer* buffer, RNE_FontInterface font) {
    for (RNE_DrawCmd* cmd = buffer->first; cmd != NULL; cmd = cmd->next) {
        if (cmd->type != RNE_DRAW_CMD_TYPE_TEXT) {
            continue;
        }

        RNE_DrawText text = cmd->data.text;
        RNE_Handle atlas = font.get_atlas(text.font_handle, text.font_size);

        SP_Vec2 gpos = text.pos;
        gpos.y += font.get_metrics(text.font_handle, text.font_size).ascent;
        for (u32 i = 0; i < text.text.len; i++) {
            RNE_Glyph glyph = font.get_glyph(text.font_handle, text.text.data[i], text.font_size);
            SP_Vec2 non_snapped = sp_v2_add(gpos, glyph.offset);
            SP_Vec2 snapped = sp_v2(floorf(non_snapped.x), floorf(non_snapped.y));

            RNE_DrawCmd* image_cmd = sp_arena_push_no_zero(buffer->arena, sizeof(RNE_DrawCmd));
            *image_cmd = (RNE_DrawCmd) {
                .type = RNE_DRAW_CMD_TYPE_IMAGE,
                    .filled = true,
                    .data.image = {
                        .pos = snapped,
                        .size = glyph.size,
                        .uv = {glyph.uv[0], glyph.uv[1]},
                        .texture_handle = atlas,
                        .color = text.color,
                    },
            };
            sp_dll_insert(buffer->first, buffer->last, image_cmd, cmd);

            gpos.x += glyph.advance;
            if (font.get_kerning != NULL && i < text.text.len - 1) {
                gpos.x += font.get_kerning(text.font_handle, text.text.data[i], text.text.data[i+1], text.font_size);
            }
        }
    }
}

struct RNE_TessellationState {
    b8 finished;
    b8 not_first_call;
    RNE_DrawCmd* current_cmd;
    RNE_DrawScissor current_scissor;
};

RNE_BatchCmd rne_tessellate(RNE_DrawCmdBuffer* buffer,
        RNE_TessellationConfig config,
        RNE_TessellationState** state) {
    RNE_TessellationState* _state = *state;
    // First call
    if (_state == NULL) {
        *state = sp_arena_push(config.arena, sizeof(RNE_TessellationState));
        _state = *state;
        _state->current_cmd = buffer->first;
        _state->current_scissor = (RNE_DrawScissor) {
            .pos = sp_v2s(-(1<<13)),
            .size = sp_v2s(1<<14),
        };
        pre_process_buffer(buffer, config.font);
    }

    if (_state->finished) {
        return (RNE_BatchCmd) {0};
    }

    RNE_RenderCmd* first = NULL;
    RNE_RenderCmd* last = NULL;

    u32 vertex_end = 0;
    u32 index_end = 0;
    u32 index_count = 0;
    u32 texture_count = 0;

    SP_Scratch scratch = sp_scratch_begin(&config.arena, 1);
    RNE_Vertex* points = sp_arena_push_no_zero(scratch.arena, sizeof(RNE_Vertex) * config.vertex_capacity);
    Path path = {
        .points = points,
    };

    for (; _state->current_cmd != NULL; _state->current_cmd = _state->current_cmd->next) {
        RNE_DrawCmd* cmd = _state->current_cmd;
        RNE_Handle texture = config.null_texture;
        switch (cmd->type) {
            case RNE_DRAW_CMD_TYPE_LINE:
                push_line(&path, cmd->data.line);
                break;
            case RNE_DRAW_CMD_TYPE_ARC:
                push_arc(&path, cmd->data.arc);
                if (cmd->filled) {
                    push_point(&path, (RNE_Vertex) {
                            .pos = cmd->data.arc.pos,
                            .color = cmd->data.arc.color,
                        });
                }
                break;
            case RNE_DRAW_CMD_TYPE_CIRCLE:
                push_circle(&path, cmd->data.circle);
                break;
            case RNE_DRAW_CMD_TYPE_RECT:
                push_rect(&path, cmd->data.rect);
                break;
            case RNE_DRAW_CMD_TYPE_IMAGE:
                push_image(&path, cmd->data.image);
                texture = cmd->data.image.texture_handle;
                break;
            case RNE_DRAW_CMD_TYPE_TEXT:
                break;
            case RNE_DRAW_CMD_TYPE_SCISSOR:
                push_render_cmd(config.arena,
                        &first,
                        &last,
                        index_end,
                        &index_count,
                        _state->current_scissor);
                _state->current_scissor = cmd->data.scissor;
                break;
        }

        i32 texture_index = find_texture(config.texture_buffer,
                    config.texture_capacity,
                    &texture_count,
                    texture);
        if (texture_index < 0) {
            push_render_cmd(config.arena,
                    &first,
                    &last,
                    index_end,
                    &index_count,
                    _state->current_scissor);
            break;
        }

        FattenResult result = path_fatten(&path, (FattenConfig) {
                .vertex_buffer = config.vertex_buffer,
                .vertex_capacity = config.vertex_capacity,
                .vertex_end = vertex_end,

                .index_buffer = config.index_buffer,
                .index_capacity = config.index_capacity,
                .index_end = index_end,

                .texture_index = texture_index,
            }, cmd);

        if (result.out_of_memory) {
            push_render_cmd(config.arena,
                    &first,
                    &last,
                    index_end,
                    &index_count,
                    _state->current_scissor);
            break;
        }

        vertex_end += result.vertex_count;
        index_end += result.index_count;
        index_count += result.index_count;
    }
    sp_scratch_end(scratch);

    if (_state->current_cmd == NULL && !_state->finished && index_count > 0) {
        push_render_cmd(config.arena,
                &first,
                &last,
                index_end,
                &index_count,
                _state->current_scissor);
        _state->finished = true;
    }

    RNE_BatchCmd batch_cmd = {
        .vertex_count = vertex_end,
        .index_count = index_end,
        .texture_count = texture_count,
        .render_cmds = first,
    };
    return batch_cmd;
}
