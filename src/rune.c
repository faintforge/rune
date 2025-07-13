#include "rune/rune.h"
#include "rune_internal.h"
#include "spire.h"

#include <stdio.h>
#include <string.h>

RNE_Context ctx = {0};

enum {
    WIDGET_DEAD,
    WIDGET_ALIVE,
};

RNE_WidgetMap rne_widget_map_init(SP_Arena* arena) {
    RNE_WidgetMap map = {
        .arena = arena,
        .widgets = {{0}},
    };
    return map;
}

static RNE_Widget* rne_widget_map_new_widget(RNE_WidgetMap* map) {
    RNE_Widget* new_widget = NULL;
    if (map->free_stack != NULL) {
        new_widget = map->free_stack;
        map->free_stack = new_widget->stack_next;
    } else {
        new_widget = sp_arena_push_no_zero(map->arena, sizeof(RNE_Widget));
    }
    *new_widget = (RNE_Widget) {
        .map_state = WIDGET_ALIVE,
    };
    return new_widget;
}

RNE_Widget* rne_widget_map_request(RNE_WidgetMap* map, u64 hash, SP_Str id) {
    // No ID
    if (id.len == 0) {
        RNE_Widget* widget = rne_widget_map_new_widget(map);
        widget->id = id;
        widget->hash = 0;
        widget->stack_next = map->no_id_stack;
        map->no_id_stack = widget;
        return widget;
    }

    u32 index = hash % WIDGET_MAP_COUNT;
    RNE_Widget* slot = &map->widgets[index];

    if (slot->map_state == WIDGET_DEAD) {
        u8* id_copy_data = sp_arena_push_no_zero(map->arena, id.len);
        memcpy(id_copy_data, id.data, id.len);
        SP_Str id_copy = sp_str(id_copy_data, id.len);

        slot->map_state = WIDGET_ALIVE;
        slot->map_last = slot;
        slot->hash = hash;
        slot->id = id_copy;
        return slot;
    }

    for (RNE_Widget* curr = slot; curr != NULL; curr = curr->map_next) {
        if (curr->hash == hash && sp_str_equal(curr->id, id)) {
            if (curr->last_touched == ctx.current_frame) {
                sp_warn("Duplicate ID:s! ('%.*s')", id.len, id.data);
                RNE_Widget* widget = rne_widget_map_new_widget(map);
                widget->id = sp_str_lit("");
                widget->hash = 0;
                widget->stack_next = map->no_id_stack;
                map->no_id_stack = widget;
                return widget;
            }
            return curr;
        }
    }

    u8* id_copy_data = sp_arena_push_no_zero(map->arena, id.len);
    memcpy(id_copy_data, id.data, id.len);
    SP_Str id_copy = sp_str(id_copy_data, id.len);

    RNE_Widget* new_widget = rne_widget_map_new_widget(map);
    new_widget->id = id_copy;
    new_widget->hash = hash;

    // Insert into back of list
    slot->map_last->map_next = new_widget;
    new_widget->map_prev = slot->map_last;
    slot->map_last = new_widget;

    return new_widget;
}

static void rne_widget_map_remove(RNE_WidgetMap* map, RNE_Widget* widget) {
    // First widget in linked list
    if (widget->map_prev == NULL) {
        if (widget->map_next == NULL) {
            widget->map_state = WIDGET_DEAD;
        } else {
            RNE_Widget* free_widget = widget->map_next;
            if (free_widget->map_next != NULL) {
                free_widget->map_next->map_prev = free_widget->map_prev;
            }

            RNE_Widget* last = widget->map_last;
            if (free_widget == last) {
                widget->map_last = free_widget->map_prev;
            }

            *widget = *free_widget;
            widget->map_last = last;
            widget->map_prev = NULL;

            free_widget->stack_next = map->free_stack;
            map->free_stack = free_widget;
            free_widget->map_state = WIDGET_DEAD;
        }
    } else {
        u32 slot_index = widget->hash % WIDGET_MAP_COUNT;
        RNE_Widget* slot = &map->widgets[slot_index];
        sp_assert(slot != widget, "Widget shouldn't be first in list!");
        if (widget == slot->map_last) {
            slot->map_last = widget->map_prev;
        }

        widget->map_prev->next = widget->map_next;
        if (widget->map_next != NULL) {
            widget->map_next->prev = widget->map_prev;
        }

        widget->stack_next = map->free_stack;
        map->free_stack = widget;
        widget->map_state = WIDGET_DEAD;
    }
}

// Needs to be called AFTER the last widget tree is done being used but BEFORE
// it has started being recreated, otherwise there will be cycles within the
// linked list.
void rne_widget_map_cleanup(RNE_WidgetMap* map) {
    while (map->no_id_stack != NULL) {
        RNE_Widget* no_id = map->no_id_stack;
        map->no_id_stack = no_id->stack_next;
        no_id->map_state = WIDGET_DEAD;

        no_id->stack_next = map->free_stack;
        map->free_stack = no_id;
    }

    RNE_Widget* remove_stack = NULL;
    for (u32 i = 0; i < WIDGET_MAP_COUNT; i++) {
        for (RNE_Widget* curr = &map->widgets[i]; curr != NULL; curr = curr->map_next) {
            if (curr->map_state == WIDGET_DEAD) {
                continue;
            }

            sp_assert(curr->id.len > 0, "No ID widgets shouldn't be in map.");
            if (curr->last_touched + 2 <= ctx.current_frame) {
                sp_sll_stack_push_nz(remove_stack, curr, stack_next, sp_null_check);
            }
        }
    }

    while (remove_stack != NULL) {
        RNE_Widget* widget = remove_stack;
        remove_stack = remove_stack->stack_next;
        rne_widget_map_remove(map, widget);
    }
}

// Prints out header code for managing widget style stacks (push, pop, next and
// top) to stdout.
void generate_header_functions(void) {
    // Push
    #define X(name_upper, name_lower, type) \
        printf("extern void rne_push_%s(%s value);\n", #name_lower, #type);
    LIST_STYLE_STACKS
    printf("\n");
    #undef X

    // Pop
    #define X(name_upper, name_lower, type) \
        printf("extern %s rne_pop_%s(void);\n", #type, #name_lower);
    LIST_STYLE_STACKS
    printf("\n");
    #undef X

    // Next
    #define X(name_upper, name_lower, type) \
        printf("extern void rne_next_%s(%s value);\n", #name_lower, #type);
    LIST_STYLE_STACKS
    printf("\n");
    #undef X


    // Top
    #define X(name_upper, name_lower, type) \
        printf("extern %s rne_top_%s(void);\n", #type, #name_lower);
    LIST_STYLE_STACKS
    #undef X
}

void rne_init(RNE_StyleStack default_style_stack, RNE_TextMeasureFunc text_measure_func) {
    ctx = (RNE_Context) {
        .arena = sp_arena_create(),
        .frame_arenas = {
            sp_arena_create(),
            sp_arena_create(),
        },
        .default_style_stack = default_style_stack,
        .text_measure = text_measure_func,
    };
    sp_arena_tag(ctx.arena, sp_str_lit("ui-state"));
    sp_arena_tag(ctx.frame_arenas[0], sp_str_lit("ui-frame-0"));
    sp_arena_tag(ctx.frame_arenas[1], sp_str_lit("ui-frame-1"));

    ctx.widget_map = rne_widget_map_init(ctx.arena);

    // generate_header_functions();
}

static void process_mouse(RNE_Mouse mouse) {
    RNE_InternalMouse old = ctx.mouse;

    for (u8 i = 0; i < RNE_MOUSE_BUTTON_COUNT; i++) {
        if (mouse.buttons[i] && !old.buttons[i].down) {
            old.buttons[i].first_frame_pressed = true;
        } else if (!mouse.buttons[i] && old.buttons[i].down) {
            old.buttons[i].first_frame_released = true;
        } else {
            old.buttons[i].first_frame_released = false;
            old.buttons[i].first_frame_pressed = false;
        }
        old.buttons[i].down = mouse.buttons[i];
    }

    SP_Vec2 pos_delta = sp_v2_sub(mouse.pos, old.pos);
    old.pos_delta = pos_delta;
    old.pos = mouse.pos;
    old.scroll = mouse.scroll;

    ctx.mouse = old;
}

static void reset_signals(RNE_Widget* widget, RNE_Widget* hit_widget) {
    if (widget == NULL) {
        return;
    }
    if (widget != hit_widget) {
        widget->signal = (RNE_Signal) {
            .focused = widget == ctx.focused_widget,
            .active = widget == ctx.active_widget,
        };
    }
    reset_signals(widget->next, hit_widget);
    reset_signals(widget->child_first, hit_widget);
}

static RNE_Widget* hit_testing(RNE_Widget* widget) {
    if (widget == NULL) {
        return NULL;
    }

    // Post-order depth-first
    RNE_Widget* hit_result = NULL;
    hit_result = hit_testing(widget->child_last);
    if (hit_result != NULL) {
        return hit_result;
    }

    SP_Vec2 mpos = ctx.mouse.pos;
    f32 left = widget->computed_absolute_position.x;
    f32 right = left + widget->computed_outer_size.x;
    f32 top = widget->computed_absolute_position.y;
    f32 bottom = top + widget->computed_outer_size.y;
    b8 hovered = mpos.x > left &&
        mpos.x < right &&
        mpos.y < bottom &&
        mpos.y > top;

    if (hovered && !(widget->flags & RNE_WIDGET_FLAG_NON_INTERACTIVE)) {
        return widget;
    }

    hit_result = hit_testing(widget->prev);
    if (hit_result != NULL) {
        return hit_result;
    }

    return NULL;
}

static void process_signal(RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    SP_Vec2 mpos = ctx.mouse.pos;
    f32 left = widget->computed_absolute_position.x;
    f32 right = left + widget->computed_outer_size.x;
    f32 top = widget->computed_absolute_position.y;
    f32 bottom = top + widget->computed_outer_size.y;
    b8 hovered = mpos.x > left &&
        mpos.x < right &&
        mpos.y < bottom &&
        mpos.y > top;

    b8 pressed = hovered && ctx.mouse.buttons[RNE_MOUSE_BUTTON_LEFT].down;

    widget->signal = (RNE_Signal) {
        .hovered = hovered,
        .just_pressed = hovered && ctx.mouse.buttons[RNE_MOUSE_BUTTON_LEFT].first_frame_pressed,
        .just_released = hovered && ctx.mouse.buttons[RNE_MOUSE_BUTTON_LEFT].first_frame_released,
        .pressed = pressed,
        .focused = widget == ctx.focused_widget,
        .active = widget == ctx.active_widget,
        .scroll = hovered && ctx.mouse.scroll,
        .drag = pressed ? ctx.mouse.pos_delta : sp_v2s(0.0f),
    };

    if (widget->flags & RNE_WIDGET_FLAG_VIEW_SCROLL) {
        f32 child_height = widget->child_size_sum.y;
        f32 view_height = widget->computed_inner_size.y;
        f32 scroll_bound = child_height - view_height;
        widget->view_offset.y += widget->signal.scroll * 32.0f;
        widget->view_offset.y = sp_clamp(widget->view_offset.y, -scroll_bound, 0.0f);
    }

    if (pressed) {
        ctx.focused_widget = widget;
        ctx.active_widget = widget;
    }
}

#define check_cyclical(widget, first_run) _check_cyclical(widget, first_run, __FILE__, __LINE__)
static void _check_cyclical(RNE_Widget* widget, b8 first_run, const char* file, u32 line) {
    if (widget == NULL) {
        return;
    }

    static RNE_Widget* already_seen[4096] = {0};
    static u32 seen_i = 0;
    if (first_run) {
        seen_i = 0;
    }

    for (u32 i = 0; i < seen_i; i++) {
        if (widget == already_seen[i]) {
            _sp_log_internal(SP_LOG_LEVEL_TRACE, file, line, "Cycle detected!");
            abort();
        }
    }

    already_seen[seen_i] = widget;
    seen_i++;

    _check_cyclical(widget->child_first, false, file, line);
    _check_cyclical(widget->next, false, file, line);
}

void rne_begin(SP_Ivec2 container_size, RNE_Mouse mouse) {
    ctx.current_frame++;

    check_cyclical(&ctx.container, true);

    SP_Arena* arena = rne_get_frame_arena();
    sp_arena_clear(arena);

    process_mouse(mouse);
    if (!ctx.mouse.buttons[RNE_MOUSE_BUTTON_LEFT].down) {
        ctx.focused_widget = NULL;
    }
    RNE_Widget* signal_widget = ctx.focused_widget;
    if (ctx.focused_widget == NULL) {
        signal_widget = hit_testing(&ctx.container);
    }
    process_signal(signal_widget);
    reset_signals(&ctx.container, signal_widget);

    rne_widget_map_cleanup(&ctx.widget_map);
    ctx.container = (RNE_Widget) {
        .id = sp_str_lit("(root_container)"),
        .size = {
            [RNE_AXIS_HORIZONTAL] = {
                .kind = RNE_SIZE_KIND_PIXELS,
                .value = container_size.x,
                .strictness = 1.0f,
            },
            [RNE_AXIS_VERTICAL] = {
                .kind = RNE_SIZE_KIND_PIXELS,
                .value = container_size.y,
                .strictness = 1.0f,
            },
        },
        .flags = RNE_WIDGET_FLAG_FIXED,
    };

    rne_push_width(ctx.default_style_stack.size[RNE_AXIS_HORIZONTAL]);
    rne_push_height(ctx.default_style_stack.size[RNE_AXIS_VERTICAL]);
    rne_push_bg(ctx.default_style_stack.bg);
    rne_push_fg(ctx.default_style_stack.fg);
    rne_push_font(ctx.default_style_stack.font);
    rne_push_font_size(ctx.default_style_stack.font_size);
    rne_push_flow(ctx.default_style_stack.flow);
    rne_push_parent(&ctx.container);
    rne_push_text_align(ctx.default_style_stack.text_align);
    rne_push_corner_radius(sp_v4s(0.0f));
    rne_push_padding(sp_v4s(0.0f));
    rne_push_offset(rne_offset(sp_v2s(0.0f), sp_v2s(0.0f)));
}

static void add_padding(RNE_Widget* widget) {
    SP_Vec4 padding = widget->padding;
    SP_Vec2 additional_size = sp_v2(padding.x + padding.z, padding.y + padding.w);
    widget->computed_outer_size = sp_v2_add(widget->computed_inner_size, additional_size);
    widget->computed_inner_position = sp_v2(padding.x, padding.y);
}

static SP_Vec2 sum_child_size(RNE_Widget* widget) {
    SP_Vec2 child_sum = sp_v2s(0.0f);
    for (RNE_Widget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
        if (!(curr_child->flags & (RNE_WIDGET_FLAG_FIXED | RNE_WIDGET_FLAG_FLOATING))) {
            child_sum.elements[widget->flow] += curr_child->computed_outer_size.elements[widget->flow];
            child_sum.elements[!widget->flow] = sp_max(child_sum.elements[!widget->flow], curr_child->computed_outer_size.elements[!widget->flow]);
        }
    }
    return child_sum;
}

static void build_intrinsic_size(RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    build_intrinsic_size(widget->next);
    build_intrinsic_size(widget->child_first);

    SP_Vec2 text_size = sp_v2s(0.0f);
    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        if (widget->size[i].kind == RNE_SIZE_KIND_TEXT) {
            text_size = ctx.text_measure(widget->font, widget->text, widget->font_size);
            break;
        }
    }

    SP_Vec2 child_sum = sp_v2s(0.0f);
    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        if (widget->size[i].kind == RNE_SIZE_KIND_CHILDREN) {
            child_sum = sum_child_size(widget);
            break;
        }
    }


    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        switch (widget->size[i].kind) {
            case RNE_SIZE_KIND_PIXELS:
                widget->computed_inner_size.elements[i] = widget->size[i].value;
                break;
            case RNE_SIZE_KIND_TEXT:
                widget->computed_inner_size.elements[i] = text_size.elements[i];
                break;
            case RNE_SIZE_KIND_CHILDREN:
                widget->computed_inner_size.elements[i] = child_sum.elements[i];
                break;
            default:
                break;
        }
    }

    add_padding(widget);
}

static void build_parent_size(RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        switch (widget->size[i].kind) {
            case RNE_SIZE_KIND_PARENT:
                widget->computed_inner_size.elements[i] = widget->parent->computed_inner_size.elements[i] * widget->size[i].value;
                break;
            default:
                break;
        }
    }
    add_padding(widget);

    build_parent_size(widget->child_first);
    build_parent_size(widget->next);
}

static void solve_size_violations(RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    solve_size_violations(widget->next);
    solve_size_violations(widget->child_first);

    SP_Vec2 child_sum = sum_child_size(widget);
    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        if (widget->flags & RNE_WIDGET_FLAG_OVERFLOW_X << i) {
            continue;
        }

        // f32 total_violation_amount = child_sum.elements[i] - widget->computed_outer_size.elements[i];
        // f32 mutable_violation_amount = child_sum.elements[i] - widget->computed_inner_size.elements[i];
        f32 violation_amount = child_sum.elements[i] - widget->computed_inner_size.elements[i];

        // Violation
        if (violation_amount > 0.0f) {
            f32 total_budget = 0.0f;
            for (RNE_Widget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
                total_budget += curr_child->computed_inner_size.elements[i] * (1.0f - curr_child->size[i].strictness);
            }

            if (total_budget < violation_amount) {
                sp_debug("%.*s - violation = %f, child_sum = %f, widget_size = %f",
                        widget->id.len,
                        widget->id.data,
                        violation_amount,
                        child_sum.elements[i],
                        widget->computed_inner_size.elements[i]);
                sp_warn("Widget '%.*s' has a sizing violation of %.0f pixels on the %s-axis.", widget->id.len, widget->id.data, violation_amount - total_budget, i ? "y" : "x");
            }

            for (RNE_Widget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
                f32 child_budget = curr_child->computed_inner_size.elements[i] * (1.0f - curr_child->size[i].strictness);
                curr_child->computed_inner_size.elements[i] -= child_budget * (violation_amount / total_budget);
                curr_child->computed_inner_size.elements[i] = floorf(curr_child->computed_inner_size.elements[i]);
                add_padding(curr_child);
            }
        }

    }
}

static void assign_child_size_sum(RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    widget->child_size_sum = sum_child_size(widget);

    assign_child_size_sum(widget->next);
    assign_child_size_sum(widget->child_first);
}

static void build_positions(RNE_Widget* widget, SP_Vec2 relative_position) {
    if (widget == NULL) {
        return;
    }

    if (widget->flags & RNE_WIDGET_FLAG_FIXED) {
        SP_Vec2 offset_amount = widget->offset.pixels;
        SP_Vec2 size = sp_v2(ctx.container.size[RNE_AXIS_HORIZONTAL].value,
                ctx.container.size[RNE_AXIS_VERTICAL].value);
        size = sp_v2_sub(size, widget->computed_outer_size);
        SP_Vec2 percent = sp_v2_mul(size, widget->offset.percent);
        offset_amount = sp_v2_add(offset_amount, percent);
        widget->computed_absolute_position = offset_amount;
    } else if (widget->flags & RNE_WIDGET_FLAG_FLOATING) {
        SP_Vec2 anchor = sp_v2s(0.0f);
        SP_Vec2 offset_amount = widget->offset.pixels;
        if (widget->parent != NULL) {
            anchor = sp_v2_add(widget->parent->computed_absolute_position, widget->parent->computed_inner_position);
            SP_Vec2 size = widget->parent->computed_inner_size;
            size = sp_v2_sub(size, widget->computed_outer_size);
            SP_Vec2 percent = sp_v2_mul(size, widget->offset.percent);
            offset_amount = sp_v2_add(offset_amount, percent);
        }
        widget->computed_absolute_position = sp_v2_add(anchor, offset_amount);
    } else {
        for (RNE_Axis axis = 0; axis < RNE_AXIS_COUNT; axis++) {
            widget->computed_relative_position = relative_position;
            if (widget->parent == NULL) {
                widget->computed_absolute_position.elements[axis] = relative_position.elements[axis];
            } else {
                RNE_Widget* parent = widget->parent;
                widget->computed_absolute_position.elements[axis] = parent->computed_absolute_position.elements[axis] + relative_position.elements[axis] + parent->view_offset.elements[axis];
                RNE_Axis flow = widget->parent->flow;
                if (flow == axis) {
                    relative_position.elements[flow] += widget->computed_outer_size.elements[flow];
                }
            }
        }
    }

    build_positions(widget->next, relative_position);
    build_positions(widget->child_first, widget->computed_inner_position);
}

void rne_end(void) {
    build_intrinsic_size(&ctx.container);
    build_parent_size(&ctx.container);
    solve_size_violations(&ctx.container);
    assign_child_size_sum(&ctx.container);
    build_positions(&ctx.container, sp_v2s(0.0f));
}

static void rne_draw_helper(RNE_DrawCmdBuffer* buffer, RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    b8 reset_scissor = false;
    if (widget->flags & RNE_WIDGET_FLAG_CLIP) {
        rne_draw_scissor(buffer, (RNE_DrawScissor) {
                .pos = widget->computed_absolute_position,
                .size = widget->computed_outer_size,
            });
        reset_scissor = true;
    }

    if (widget->flags & RNE_WIDGET_FLAG_DRAW_BACKGROUND) {
        rne_draw_rect_filled(buffer, (RNE_DrawRect) {
                .pos = widget->computed_absolute_position,
                .size = widget->computed_outer_size,
                .color = widget->bg,
                .corner_radius = widget->corner_radius,
                .corner_segments = 8,
            });
    }

    if (widget->flags & RNE_WIDGET_FLAG_DRAW_TEXT) {
        SP_Vec2 pos = sp_v2_add(widget->computed_absolute_position, widget->computed_inner_position);
        SP_Vec2 text_size = ctx.text_measure(widget->font, widget->text, widget->font_size);
        switch (widget->text_align) {
            case RNE_TEXT_ALIGN_LEFT:
                break;
            case RNE_TEXT_ALIGN_CENTER: {
                pos.x += widget->computed_inner_size.x / 2.0f;
                pos.x -= text_size.x / 2.0f;
            } break;
            case RNE_TEXT_ALIGN_RIGHT: {
                pos.x += widget->computed_inner_size.x;
                pos.x -= text_size.x;
            } break;
        }

        // Center text vertically
        pos.y += widget->computed_inner_size.y / 2.0f;
        pos.y -= text_size.y / 2.0f;

        rne_draw_text(buffer, (RNE_DrawText) {
                .pos = pos,
                .text = widget->text,
                .font_handle = widget->font,
                .font_size = widget->font_size,
                .color = widget->fg,
            });
    }

    if (widget->render_func != NULL) {
        widget->render_func(buffer, widget, widget->render_userdata);
    }

    // Depth-first
    rne_draw_helper(buffer, widget->child_first);

    if (reset_scissor) {
        rne_draw_scissor(buffer, (RNE_DrawScissor) {
                .pos = sp_v2s(-(1<<13)),
                .size = sp_v2s(1<<14),
            });
    }
    rne_draw_helper(buffer, widget->next);
}

RNE_DrawCmdBuffer rne_draw(SP_Arena* arena) {
    RNE_DrawCmdBuffer buffer = rne_draw_buffer_begin(arena);
    rne_draw_helper(&buffer, &ctx.container);
    return buffer;
}

SP_Arena* rne_get_frame_arena(void) {
    return ctx.frame_arenas[ctx.current_frame % sp_arrlen(ctx.frame_arenas)];
}

static void parse_text(SP_Str text, SP_Str* id, SP_Str* display_text) {
    *id = text;
    *display_text = text;

    // Don't include anything after ## in display text.
    if (text.len < 2) {
        return;
    }
    for (u32 i = 0; i < text.len - 2; i++) {
        if (text.data[i] == '#' && text.data[i + 1] == '#') {
            *display_text = sp_str_substr(text, 0, i);
            break;
        }
    }


    // Only use text after ### for id.
    if (text.len < 3) {
        return;
    }
    for (u32 i = 0; i < text.len - 3; i++) {
        if (text.data[i] == '#' && text.data[i + 1] == '#' && text.data[i + 2] == '#') {
            *id = sp_str_substr(text, i, text.len);
            break;
        }
    }
}

RNE_Widget* rne_widget(SP_Str text, RNE_WidgetFlags flags) {
    SP_Arena* arena = rne_get_frame_arena();
    SP_Str text_copy = sp_str_pushf(sp_arena_allocator(arena), "%.*s", text.len, text.data);
    SP_Str id, display_text;
    parse_text(text_copy, &id, &display_text);

    RNE_Widget* parent = rne_top_parent();
    u64 hash = parent->hash + sp_fvn1a_hash(id.data, id.len);
    RNE_Widget* widget = rne_widget_map_request(&ctx.widget_map, hash, id);
    *widget = (RNE_Widget) {
        .parent = parent,

        .next = NULL,
        .prev = NULL,

        .child_first = NULL,
        .child_last = NULL,

        .hash = widget->hash,
        .id = widget->id,
        .map_state = widget->map_state,
        .map_next = widget->map_next,
        .map_prev = widget->map_prev,
        .map_last = widget->map_last,
        .stack_next = widget->stack_next,

        .flags = flags,
        .size = {
            rne_top_width(),
            rne_top_height(),
        },

        // Retain possible state from the last frame
        .computed_relative_position = widget->computed_relative_position,
        .computed_absolute_position = widget->computed_absolute_position,
        .computed_inner_position = widget->computed_inner_position,
        .computed_outer_size = widget->computed_outer_size,
        .computed_inner_size = widget->computed_inner_size,
        .view_offset = widget->view_offset,
        .child_size_sum = widget->child_size_sum,
        .signal = widget->signal,

        .text = display_text,
        .last_touched = ctx.current_frame,

        .bg = rne_top_bg(),
        .fg = rne_top_fg(),
        .font = rne_top_font(),
        .font_size = rne_top_font_size(),
        .flow = rne_top_flow(),
        .text_align = rne_top_text_align(),
        .corner_radius = rne_top_corner_radius(),
        .padding = rne_top_padding(),
        .offset = rne_top_offset(),
    };

    sp_dll_push_back(parent->child_first, parent->child_last, widget);

    check_cyclical(&ctx.container, true);

    return widget;
}

void rne_widget_equip_render_func(RNE_Widget* widget, RNE_WidgetRenderFunc func, void* userdata) {
    widget->render_func = func;
    widget->render_userdata = userdata;
}

RNE_Signal rne_signal(RNE_Widget* widget) {
    return widget->signal;
}

RNE_Offset rne_offset(SP_Vec2 pixels, SP_Vec2 percent) {
    return (RNE_Offset) {
        .pixels = pixels,
        .percent = percent,
    };
}

// Push impls
#define X(name_upper, name_lower, type) \
    void rne_push_##name_lower(type value) { \
        SP_Arena* arena = rne_get_frame_arena(); \
        RNE_##name_upper##Node* node = sp_arena_push_no_zero(arena, sizeof(RNE_##name_upper##Node)); \
        *node = (RNE_##name_upper##Node) { \
            .value = value, \
            .next = NULL, \
            .pop_next = false, \
        }; \
        sp_sll_stack_push(ctx.name_lower##_stack, node); \
    }
LIST_STYLE_STACKS
#undef X

// Pop impls
#define X(name_upper, name_lower, type) \
    type rne_pop_##name_lower(void) { \
        sp_assert(ctx.name_lower##_stack->next != NULL, "Too many pops on the %s stack!", #name_lower); \
        type value = ctx.name_lower##_stack->value; \
        sp_sll_stack_pop(ctx.name_lower##_stack); \
        return value; \
    }
LIST_STYLE_STACKS
#undef X

// Push impls
#define X(name_upper, name_lower, type) \
    void rne_next_##name_lower(type value) { \
        SP_Arena* arena = rne_get_frame_arena(); \
        RNE_##name_upper##Node* node = sp_arena_push_no_zero(arena, sizeof(RNE_##name_upper##Node)); \
        *node = (RNE_##name_upper##Node) { \
            .value = value, \
            .next = NULL, \
            .pop_next = true, \
        }; \
        sp_sll_stack_push(ctx.name_lower##_stack, node); \
    }
LIST_STYLE_STACKS
#undef X

// Top impls
#define X(name_upper, name_lower, type) \
    type rne_top_##name_lower(void) { \
        sp_assert(ctx.name_lower##_stack != NULL, "All " #name_lower " have been popped off of the style stack."); \
        type value = ctx.name_lower##_stack->value; \
        if (ctx.name_lower##_stack->pop_next) { \
            sp_sll_stack_pop(ctx.name_lower##_stack); \
        } \
        return value; \
    }
LIST_STYLE_STACKS
#undef X

// -- Drawing ------------------------------------------------------------------

RNE_DrawCmdBuffer rne_draw_buffer_begin(SP_Arena* arena) {
    return (RNE_DrawCmdBuffer) {
        .arena = arena,
    };
}

// Pushes a draw command onto the end of the draw buffer.
static void rne_draw_buffer_push(RNE_DrawCmdBuffer* buffer, RNE_DrawCmd cmd) {
    RNE_DrawCmd* cmd_copy = sp_arena_push_no_zero(buffer->arena, sizeof(RNE_DrawCmd));
    *cmd_copy = cmd;
    sp_sll_queue_push(buffer->first, buffer->last, cmd_copy);
}

void rne_draw_line(RNE_DrawCmdBuffer* buffer, RNE_DrawLine line) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_LINE,
            .filled = false,
            .closed = false,
            .thickness = line.thickness,
            .data.line = line,
        });
}

void rne_draw_arc_filled(RNE_DrawCmdBuffer* buffer, RNE_DrawArc arc) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_ARC,
            .filled = true,
            .data.arc = arc,
        });
}

void rne_draw_arc_stroke(RNE_DrawCmdBuffer* buffer, RNE_DrawArc arc, f32 thickness) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_ARC,
            .filled = false,
            .closed = false,
            .thickness = thickness,
            .data.arc = arc,
        });
}

void rne_draw_circle_filled(RNE_DrawCmdBuffer* buffer, RNE_DrawCircle circle) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_CIRCLE,
            .filled = true,
            .data.circle = circle,
        });
}

void rne_draw_circle_stroke(RNE_DrawCmdBuffer* buffer, RNE_DrawCircle circle, f32 thickness) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_CIRCLE,
            .filled = false,
            .closed = false,
            .thickness = thickness,
            .data.circle = circle,
        });
}

void rne_draw_rect_filled(RNE_DrawCmdBuffer* buffer, RNE_DrawRect rect) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_RECT,
            .filled = true,
            .data.rect = rect,
        });
}

void rne_draw_rect_stroke(RNE_DrawCmdBuffer* buffer, RNE_DrawRect rect, f32 thickness) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_RECT,
            .filled = false,
            .closed = true,
            .thickness = thickness,
            .data.rect = rect,
        });
}

void rne_draw_image(RNE_DrawCmdBuffer* buffer, RNE_DrawImage image) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_IMAGE,
            .filled = true,
            .data.image = image,
        });
}

void rne_draw_text(RNE_DrawCmdBuffer* buffer, RNE_DrawText text) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_TEXT,
            .data.text = text,
        });
}

void rne_draw_scissor(RNE_DrawCmdBuffer* buffer, RNE_DrawScissor scissor) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_SCISSOR,
            .data.scissor = scissor,
        });
}
