#include "rune.h"
#include "spire.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Style stacks
// #define X(name_upper, name_lower, type)
#define LIST_STYLE_STACKS \
    X(Width, width, RNE_Size) \
    X(Height, height, RNE_Size) \
    X(Bg, bg, SP_Color) \
    X(Fg, fg, SP_Color) \
    X(Font, font, RNE_Handle) \
    X(FontSize, font_size, u32) \
    X(Flow, flow, RNE_Axis) \
    X(Parent, parent, RNE_Widget*) \
    X(FixedX, fixed_x, f32) \
    X(FixedY, fixed_y, f32) \
    X(TextAlign, text_align, RNE_TextAlign)

#define X(name_upper, name_lower, type) \
    typedef struct RNE_##name_upper##Node RNE_##name_upper##Node; \
    struct RNE_##name_upper##Node { \
        RNE_##name_upper##Node* next; \
        type value; \
        b8 pop_next; \
    };
LIST_STYLE_STACKS
#undef X

#define X(name_upper, name_lower, type) RNE_##name_upper##Node* name_lower##_stack;
typedef struct RNE_Context RNE_Context;
struct RNE_Context {
    SP_Arena* arena;
    SP_Arena* frame_arenas[2];
    RNE_Widget container;
    u64 current_frame;

    SP_HashMap* widget_map;
    RNE_Widget* widget_no_id_stack;
    RNE_Widget* widget_free_stack;

    RNE_Widget* focused_widget;
    RNE_Mouse mouse;

    RNE_FontInterface font;

    // Styles
    RNE_StyleStack default_style_stack;
    LIST_STYLE_STACKS
};
#undef X

static RNE_Context ctx = {0};

static void generate_header_functions(void) {
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

void rne_init(RNE_FontInterface font, RNE_StyleStack default_style_stack) {
    ctx = (RNE_Context) {
        .arena = sp_arena_create(),
        .frame_arenas = {
            sp_arena_create(),
            sp_arena_create(),
        },
        .font = font,
        .default_style_stack = default_style_stack,
    };
    sp_arena_tag(ctx.arena, sp_str_lit("ui-state"));
    sp_arena_tag(ctx.frame_arenas[0], sp_str_lit("ui-frame-0"));
    sp_arena_tag(ctx.frame_arenas[1], sp_str_lit("ui-frame-1"));

    ctx.widget_map = sp_hm_new(sp_hm_desc_str(ctx.arena, 4096, RNE_Widget*));

    // generate_header_functions();
}

void rne_begin(SP_Ivec2 container_size, RNE_Mouse mouse) {
    SP_Arena* arena = rne_get_arena();
    sp_arena_clear(arena);

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
        .flags = RNE_WIDGET_FLAG_FLOATING,
    };
    ctx.mouse = mouse;
    if (!ctx.mouse.buttons[RNE_MOUSE_BUTTON_LEFT].pressed) {
        ctx.focused_widget = NULL;
    }

    rne_push_width(ctx.default_style_stack.size[RNE_AXIS_HORIZONTAL]);
    rne_push_height(ctx.default_style_stack.size[RNE_AXIS_VERTICAL]);
    rne_push_bg(ctx.default_style_stack.bg);
    rne_push_fg(ctx.default_style_stack.fg);
    rne_push_font(ctx.default_style_stack.font);
    rne_push_font_size(ctx.default_style_stack.font_size);
    rne_push_flow(ctx.default_style_stack.flow);
    rne_push_parent(&ctx.container);
    rne_push_fixed_x(0.0f);
    rne_push_fixed_y(0.0f);
    rne_push_text_align(ctx.default_style_stack.text_align);
}

static void build_fixed_sizes(RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    SP_Vec2 text_size = sp_v2s(0.0f);
    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        if (widget->size[i].kind == RNE_SIZE_KIND_TEXT) {
            text_size = ctx.font.measure(widget->font, widget->text, widget->font_size);
            break;
        }
    }

    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        switch (widget->size[i].kind) {
            case RNE_SIZE_KIND_PIXELS:
                widget->computed_size.elements[i] = widget->size[i].value;
                break;
            case RNE_SIZE_KIND_TEXT:
                widget->computed_size.elements[i] = text_size.elements[i];
                break;
            default:
                break;
        }
    }

    // Bredth-first
    build_fixed_sizes(widget->next);
    build_fixed_sizes(widget->child_first);
}

static SP_Vec2 sum_child_size(RNE_Widget* widget) {
    SP_Vec2 child_sum = sp_v2s(0.0f);
    for (RNE_Widget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
        if (!(curr_child->flags & RNE_WIDGET_FLAG_FLOATING_X << widget->flow)) {
            child_sum.elements[widget->flow] += curr_child->computed_size.elements[widget->flow];
            child_sum.elements[!widget->flow] = sp_max(child_sum.elements[!widget->flow], curr_child->computed_size.elements[!widget->flow]);
        }
    }
    return child_sum;
}

static void build_child_sizes(RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    // Depth-first
    build_child_sizes(widget->child_first);
    build_child_sizes(widget->next);

    SP_Vec2 child_sum = sp_v2s(0.0f);
    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        if (widget->size[i].kind == RNE_SIZE_KIND_CHILDREN) {
            child_sum = sum_child_size(widget);
            break;
        }
    }

    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        if (widget->size[i].kind == RNE_SIZE_KIND_CHILDREN) {
            widget->computed_size.elements[i] = child_sum.elements[i];
        }
    }
}

static void build_parent_sizes(RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        if (widget->size[i].kind == RNE_SIZE_KIND_PARENT) {
            sp_assert(widget->parent != NULL, "Only (root-container) should have a NULL parent.");
            widget->computed_size.elements[i] = widget->parent->computed_size.elements[i] * widget->size[i].value;
        }
    }

    // Bredth-first
    build_parent_sizes(widget->child_first);
    build_parent_sizes(widget->next);
}

static void solve_size_violations(RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    // Depth-first
    solve_size_violations(widget->next);
    solve_size_violations(widget->child_first);

    SP_Vec2 child_sum = sum_child_size(widget);
    for (u8 i = 0; i < RNE_AXIS_COUNT; i++) {
        if (widget->flags & RNE_WIDGET_FLAG_OVERFLOW_X << i) {
            continue;
        }

        f32 violation_amount = child_sum.elements[i] - widget->computed_size.elements[i];

        // Violation
        if (violation_amount > 0.0f) {
            f32 total_budget = 0.0f;
            for (RNE_Widget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
                total_budget += curr_child->computed_size.elements[i] * (1.0f - curr_child->size[i].strictness);
            }

            if (total_budget < violation_amount) {
                sp_debug("%.*s - violation = %f, child_sum = %f, widget_size = %f",
                        widget->id.len,
                        widget->id.data,
                        violation_amount,
                        child_sum.elements[i],
                        widget->computed_size.elements[i]);
                sp_warn("Widget '%.*s' has a sizing violation of %.0f pixels on the %s-axis.", widget->id.len, widget->id.data, violation_amount - total_budget, i ? "y" : "x");
            }

            for (RNE_Widget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
                f32 child_budget = curr_child->computed_size.elements[i] * (1.0f - curr_child->size[i].strictness);
                curr_child->computed_size.elements[i] -= child_budget * (violation_amount / total_budget);
                curr_child->computed_size.elements[i] = floorf(curr_child->computed_size.elements[i]);
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

    for (RNE_Axis axis = 0; axis < RNE_AXIS_COUNT; axis++) {
        if (!(widget->flags & RNE_WIDGET_FLAG_FLOATING_X << axis)) {
            widget->computed_relative_position = relative_position;
            if (widget->parent == NULL) {
                widget->computed_absolute_position.elements[axis] = relative_position.elements[axis];
            } else {
                RNE_Widget* parent = widget->parent;
                widget->computed_absolute_position.elements[axis] = parent->computed_absolute_position.elements[axis] + relative_position.elements[axis] + parent->view_offset.elements[axis];
                RNE_Axis flow = widget->parent->flow;
                if (flow == axis) {
                    relative_position.elements[flow] += widget->computed_size.elements[flow];
                }
            }
        }
    }

    build_positions(widget->next, relative_position);
    build_positions(widget->child_first, sp_v2s(0.0f));
}

void rne_end(void) {
    ctx.current_frame++;

    build_fixed_sizes(&ctx.container);
    build_child_sizes(&ctx.container);
    build_parent_sizes(&ctx.container);
    solve_size_violations(&ctx.container);
    assign_child_size_sum(&ctx.container);
    build_positions(&ctx.container, sp_v2s(0.0f));

    RNE_Widget* remove_stack = NULL;
    for (SP_HashMapIter i = sp_hm_iter_new(ctx.widget_map);
            sp_hm_iter_valid(i);
            i = sp_hm_iter_next(i)) {
        RNE_Widget* widget = sp_hm_iter_get_value(i, RNE_Widget*);
        sp_assert(widget->id.len != 0, "No ID widgets shouldn't be in the map.");
        if (widget->last_touched + 2 <= ctx.current_frame) {
            sp_sll_stack_push_nz(remove_stack, widget, stack_next, sp_null_check);
        }
    }

    while (remove_stack != NULL) {
        RNE_Widget* widget = remove_stack;
        RNE_Widget* removed = sp_hm_remove(ctx.widget_map, widget->id, RNE_Widget*);
        sp_assert(removed == widget, "Widget (%.*s) removed from map doesn't match dead widget. %p, %p", widget->id.len, widget->id.data, widget, removed);
        sp_sll_stack_pop_nz(remove_stack, stack_next, sp_null_check);
        sp_sll_stack_push_nz(ctx.widget_free_stack, widget, stack_next, sp_null_check);
    }

    while (ctx.widget_no_id_stack != NULL) {
        RNE_Widget* widget = ctx.widget_no_id_stack;
        if (widget->id.len != 0) {
            sp_debug("Uhhhhh");
        }
        sp_sll_stack_pop_nz(ctx.widget_no_id_stack, stack_next, sp_null_check);
        sp_sll_stack_push_nz(ctx.widget_free_stack, widget, stack_next, sp_null_check);
    }
}

static void rne_draw_helper(RNE_DrawCmdBuffer* buffer, RNE_Widget* widget) {
    if (widget == NULL) {
        return;
    }

    b8 reset_scissor = false;
    if (widget->flags & RNE_WIDGET_FLAG_CLIP) {
        rne_draw_scissor(buffer, (RNE_DrawScissor) {
                .pos = widget->computed_absolute_position,
                .size = widget->computed_size,
            });
        reset_scissor = true;
    }

    if (widget->flags & RNE_WIDGET_FLAG_DRAW_BACKGROUND) {
        rne_draw_rect_filled(buffer, (RNE_DrawRect) {
                .pos = widget->computed_absolute_position,
                .size = widget->computed_size,
                .color = widget->bg,
            });
    }

    if (widget->flags & RNE_WIDGET_FLAG_DRAW_TEXT) {
        SP_Vec2 pos = widget->computed_absolute_position;
        SP_Vec2 text_size = ctx.font.measure(widget->font, widget->text, widget->font_size);
        switch (widget->text_align) {
            case RNE_TEXT_ALIGN_LEFT:
                break;
            case RNE_TEXT_ALIGN_CENTER: {
                pos.x += widget->computed_size.x / 2.0f;
                pos.x -= text_size.x / 2.0f;
            } break;
            case RNE_TEXT_ALIGN_RIGHT: {
                pos.x += widget->computed_size.x;
                pos.x -= text_size.x;
            } break;
        }

        // Center text vertically
        pos.y += widget->computed_size.y / 2.0f;
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

SP_Arena* rne_get_arena(void) {
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

static RNE_Widget* new_widget(void) {
    RNE_Widget* widget;
    if (ctx.widget_free_stack != NULL) {
        widget = ctx.widget_free_stack;
        sp_sll_stack_pop_nz(ctx.widget_free_stack, stack_next, sp_null_check);
        widget->stack_next = NULL;
    } else {
        widget = sp_arena_push(ctx.arena, sizeof(RNE_Widget));
    }
    return widget;
}

static RNE_Widget* widget_from_id(SP_Str* id) {
    RNE_Widget* widget = sp_hm_get(ctx.widget_map, *id, RNE_Widget*);

    // New ID
    if (widget == NULL) {
        widget = new_widget();
        if (id->len == 0) {
            *id = sp_str_lit("");
            sp_sll_stack_push_nz(ctx.widget_no_id_stack, widget, stack_next, sp_null_check);
        } else {
            // ID must be alive as long as the widget is.
            u8* id_data = sp_arena_push_no_zero(ctx.arena, id->len);
            memcpy(id_data, id->data, id->len);
            *id = sp_str(id_data, id->len);
            sp_hm_insert(ctx.widget_map, *id, widget);
        }
    }
    // Duplicate ID
    else if (widget->last_touched == ctx.current_frame) {
        *id = sp_str_lit("");
        sp_sll_stack_push_nz(ctx.widget_no_id_stack, widget, stack_next, sp_null_check);
    }

    return widget;
}

RNE_Widget* rne_widget(SP_Str text, RNE_WidgetFlags flags) {
    SP_Arena* arena = rne_get_arena();
    SP_Str text_copy = sp_str_pushf(arena, "%.*s", text.len, text.data);
    SP_Str id, display_text;
    parse_text(text_copy, &id, &display_text);

    RNE_Widget* widget = widget_from_id(&id);

    RNE_Widget* parent = rne_top_parent();
    *widget = (RNE_Widget) {
        .parent = parent,
        .flags = flags,
        .id = id,
        .text = display_text,
        .last_touched = ctx.current_frame,
        .stack_next = widget->stack_next,

        .size = {
            rne_top_width(),
            rne_top_height(),
        },

        // Retain possible state from the last frame
        .computed_absolute_position = widget->computed_absolute_position,
        .computed_size = widget->computed_size,
        .view_offset = widget->view_offset,
        .child_size_sum = widget->child_size_sum,

        .bg = rne_top_bg(),
        .fg = rne_top_fg(),
        .font = rne_top_font(),
        .font_size = rne_top_font_size(),
        .flow = rne_top_flow(),
        .text_align = rne_top_text_align(),
    };

    if (flags & RNE_WIDGET_FLAG_FLOATING_X) {
        widget->computed_absolute_position.x = rne_top_fixed_x();
    }

    if (flags & RNE_WIDGET_FLAG_FLOATING_Y) {
        widget->computed_absolute_position.y = rne_top_fixed_y();
    }

    sp_dll_push_back(parent->child_first, parent->child_last, widget);
    return widget;
}

void rne_widget_equip_render_func(RNE_Widget* widget, RNE_WidgetRenderFunc func, void* userdata) {
    widget->render_func = func;
    widget->render_userdata = userdata;
}

RNE_Signal rne_signal(RNE_Widget* widget) {
    if (!(widget->flags & RNE_WIDGET_FLAG_INTERACTIVE)) {
        return (RNE_Signal) {0};
    }

    // TODO: Occlusion. Two interactive elements on top of eachother isn't
    // handled right now.

    SP_Vec2 mpos = ctx.mouse.pos;
    f32 left = widget->computed_absolute_position.x;
    f32 right = left + widget->computed_size.x;
    f32 top = widget->computed_absolute_position.y;
    f32 bottom = top + widget->computed_size.y;

    b8 hovered = mpos.x > left && mpos.x < right && mpos.y < bottom && mpos.y > top;
    b8 pressed = hovered && ctx.mouse.buttons[RNE_MOUSE_BUTTON_LEFT].pressed;
    if (pressed && ctx.focused_widget == NULL) {
        ctx.focused_widget = widget;
    }
    b8 focused = widget == ctx.focused_widget;
    b8 clicked = hovered && ctx.mouse.buttons[RNE_MOUSE_BUTTON_LEFT].clicked;
    SP_Vec2 drag = sp_v2s(0.0f);
    if (focused) {
        drag = ctx.mouse.pos_delta;
    }
    f32 scroll = 0.0f;
    if (hovered) {
        scroll = ctx.mouse.scroll;
    }

    if (widget->flags & RNE_WIDGET_FLAG_VIEW_SCROLL) {
        f32 child_height = widget->child_size_sum.y;
        f32 view_height = widget->computed_size.y;
        f32 scroll_bound = child_height - view_height;
        widget->view_offset.y += scroll * 32.0f;
        widget->view_offset.y = sp_clamp(widget->view_offset.y, -scroll_bound, 0.0f);
    }

    RNE_Signal signal = {
        .hovered = hovered,
        .pressed = pressed,
        .clicked = clicked,
        .focused = focused,
        .drag = drag,
        .scroll = scroll,
    };
    return signal;
}

// Push impls
#define X(name_upper, name_lower, type) \
    void rne_push_##name_lower(type value) { \
        SP_Arena* arena = rne_get_arena(); \
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
        SP_Arena* arena = rne_get_arena(); \
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

void rne_draw_buffer_push(RNE_DrawCmdBuffer* buffer, RNE_DrawCmd cmd) {
    RNE_DrawCmd* cmd_copy = sp_arena_push_no_zero(buffer->arena, sizeof(RNE_DrawCmd));
    *cmd_copy = cmd;
    sp_sll_queue_push(buffer->first, buffer->last, cmd_copy);
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

void rne_draw_line(RNE_DrawCmdBuffer* buffer, RNE_DrawLine line) {
    rne_draw_buffer_push(buffer, (RNE_DrawCmd) {
            .type = RNE_DRAW_CMD_TYPE_LINE,
            .filled = false,
            .closed = false,
            .thickness = line.thickness,
            .data.line = line,
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

// -- Tessellation -------------------------------------------------------------

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
    rect.corner_radius = sp_clamp(rect.corner_radius, 0.0f, min_side);

    // TODO: UVs
    if (rect.corner_radius == 0.0f) {
        // Top left
        push_point(path, (RNE_Vertex) { .pos = sp_v2(rect.pos.x, rect.pos.y), .color = rect.color });
        // Bottom left
        push_point(path, (RNE_Vertex) { .pos = sp_v2(rect.pos.x, rect.pos.y + rect.size.y), .color = rect.color });
        // Bottom right
        push_point(path, (RNE_Vertex) { .pos = sp_v2(rect.pos.x + rect.size.x, rect.pos.y + rect.size.y), .color = rect.color });
        // Top right
        push_point(path, (RNE_Vertex) { .pos = sp_v2(rect.pos.x + rect.size.x, rect.pos.y), .color = rect.color });
        return;
    }

    f32 inner_left   = rect.pos.x + rect.corner_radius;
    f32 inner_right  = rect.pos.x + rect.size.x - rect.corner_radius;
    f32 inner_bottom = rect.pos.y + rect.size.y - rect.corner_radius;
    f32 inner_top    = rect.pos.y + rect.corner_radius;

    // Top left
    push_arc(path, (RNE_DrawArc) {
            .pos = sp_v2(inner_left, inner_top),
            .radius = rect.corner_radius,
            .start_angle = PI / 2.0f,
            .end_angle = PI,
            .color = rect.color,
            .segments = rect.corner_segments,
        });
    // Bottom left
    push_arc(path, (RNE_DrawArc) {
            .pos = sp_v2(inner_left, inner_bottom),
            .radius = rect.corner_radius,
            .start_angle = PI,
            .end_angle = 3.0f * PI / 2.0f,
            .color = rect.color,
            .segments = rect.corner_segments,
        });
    // Bottom right
    push_arc(path, (RNE_DrawArc) {
            .pos = sp_v2(inner_right, inner_bottom),
            .radius = rect.corner_radius,
            .start_angle = -PI / 2.0f,
            .end_angle = 0.0f,
            .color = rect.color,
            .segments = rect.corner_segments,
        });
    // Top right
    push_arc(path, (RNE_DrawArc) {
            .pos = sp_v2(inner_right, inner_top),
            .radius = rect.corner_radius,
            .start_angle = 0.0f,
            .end_angle = PI / 2.0f,
            .color = rect.color,
            .segments = rect.corner_segments,
        });
}

typedef struct FattenConfig FattenConfig;
struct FattenConfig {
    RNE_Vertex* vertex_buffer;
    u16* index_buffer;

    u32 vertex_capacity;
    u32 index_capacity;

    u32 vertex_end;
    u32 index_end;
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

    u32 needed_vertices = path->point_i;
    u32 needed_indices = (path->point_i - 2) * 3;

    sp_assert(config.vertex_capacity - config.vertex_end >= needed_vertices, "Too many vertices for buffer!");
    sp_assert(config.index_capacity - config.index_end >= needed_indices, "Too many indices for buffer!");

    u32 start_offset = config.vertex_end;
    u32 point_count = path->point_i;
    for (u32 i = 0; i < point_count; i++) {
        config.vertex_buffer[start_offset + i] = path->points[i];
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

    u32 needed_vertices = path->point_i * 2;
    u32 needed_indices = closed ? path->point_i * 6 : (path->point_i - 1) * 6;

    sp_assert(config.vertex_capacity - config.vertex_end >= needed_vertices, "Too many vertices for buffer!");
    sp_assert(config.index_capacity - config.index_end >= needed_indices, "Too many indices for buffer!");

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

RNE_BatchCmd rne_tessellate(RNE_DrawCmdBuffer* buffer,
        RNE_TessellationConfig config,
        RNE_TessellationState* state) {
    // Build shape
    // Check if vbuff and ibuff has capacity for it
    // If not, return current batch

    // TODO: Text rendering

    if (!state->not_first_call) {
        state->not_first_call = true;
        state->current_cmd = buffer->first;
        state->current_scissor = (RNE_DrawScissor) {
            .pos = sp_v2s(-(1<<13)),
            .size = sp_v2s(1<<14),
        };
    }

    RNE_RenderCmd* first = NULL;
    RNE_RenderCmd* last = NULL;

    u32 vertex_end = 0;
    u32 index_end = 0;
    u32 index_count = 0;

    SP_Scratch scratch = sp_scratch_begin(&config.arena, 1);
    RNE_Vertex* points = sp_arena_push_no_zero(scratch.arena, sizeof(RNE_Vertex) * config.vertex_capacity);
    Path path = {
        .points = points,
    };

    for (; state->current_cmd != NULL; state->current_cmd = state->current_cmd->next) {
        RNE_DrawCmd* cmd = state->current_cmd;
        switch (cmd->type) {
            case RNE_DRAW_CMD_TYPE_LINE: {
                push_line(&path, cmd->data.line);
            } break;
            case RNE_DRAW_CMD_TYPE_ARC:
                push_arc(&path, cmd->data.arc);
                break;
            case RNE_DRAW_CMD_TYPE_CIRCLE:
                push_circle(&path, cmd->data.circle);
                break;
            case RNE_DRAW_CMD_TYPE_RECT:
                push_rect(&path, cmd->data.rect);
                break;
            case RNE_DRAW_CMD_TYPE_TEXT:
                break;
            case RNE_DRAW_CMD_TYPE_SCISSOR: {
                RNE_RenderCmd* render_cmd = sp_arena_push_no_zero(config.arena, sizeof(RNE_RenderCmd));
                *render_cmd = (RNE_RenderCmd) {
                    .start_offset_bytes = (index_end - index_count) * sizeof(u16),
                    .index_count = index_count,
                    .scissor = cmd->data.scissor,
                };
                sp_sll_queue_push(first, last, render_cmd);
            } break;
        }

        FattenResult result = path_fatten(&path, (FattenConfig) {
                .vertex_buffer = config.vertex_buffer,
                .index_buffer = config.index_buffer,

                .vertex_end = vertex_end,
                .index_end = index_end,

                .vertex_capacity = config.vertex_capacity,
                .index_capacity = config.index_capacity,
            }, cmd);

        if (result.out_of_memory) {
            RNE_RenderCmd* render_cmd = sp_arena_push_no_zero(config.arena, sizeof(RNE_RenderCmd));
            *render_cmd = (RNE_RenderCmd) {
                .start_offset_bytes = (index_end - index_count) * sizeof(u16),
                    .index_count = index_count,
                    .scissor = state->current_scissor,
            };
            sp_sll_queue_push(first, last, render_cmd);
            index_count = 0;
            break;
        }

        vertex_end += result.vertex_count;
        index_end += result.index_count;
        index_count += result.index_count;
    }
    sp_scratch_end(scratch);

    if (state->current_cmd == NULL && !state->finished) {
        RNE_RenderCmd* render_cmd = sp_arena_push_no_zero(config.arena, sizeof(RNE_RenderCmd));
        *render_cmd = (RNE_RenderCmd) {
            .start_offset_bytes = (index_end - index_count) * sizeof(u16),
                .index_count = index_count,
                .scissor = state->current_scissor,
        };
        sp_sll_queue_push(first, last, render_cmd);
        index_count = 0;
        state->finished = true;
    }

    RNE_BatchCmd batch_cmd = {
        .index_count = index_end,
        .vertex_count = vertex_end,
        .render_cmds = first,
    };
    return batch_cmd;
}
