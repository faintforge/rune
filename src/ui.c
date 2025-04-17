#include "ui.h"
#include "font.h"
#include "renderer.h"
#include "spire.h"

#include <math.h>
#include <stdio.h>

// Style stacks
// #define X(name_upper, name_lower, type)
#define LIST_STYLE_STACKS \
    X(Width, width, UISize) \
    X(Height, height, UISize) \
    X(Bg, bg, SP_Vec4) \
    X(Fg, fg, SP_Vec4) \
    X(Font, font, Font*) \
    X(FontSize, font_size, u32) \
    X(Flow, flow, UIAxis) \
    X(Parent, parent, UIWidget*) \
    X(FixedX, fixed_x, f32) \
    X(FixedY, fixed_y, f32)

#define X(name_upper, name_lower, type) \
    typedef struct UI##name_upper##Node UI##name_upper##Node; \
    struct UI##name_upper##Node { \
        UI##name_upper##Node* next; \
        type value; \
        b8 pop_next; \
    };
LIST_STYLE_STACKS
#undef X

#define X(name_upper, name_lower, type) UI##name_upper##Node* name_lower##_stack;
typedef struct UIContext UIContext;
struct UIContext {
    SP_Arena* arena;
    SP_Arena* frame_arenas[2];
    UIWidget container;
    u64 current_frame;

    SP_HashMap* widget_map;
    UIMouse mouse;

    // Styles
    UIStyleStack default_style_stack;
    LIST_STYLE_STACKS
};
#undef X

static UIContext ctx = {0};

static void generate_header_functions(void) {
    // Push
    #define X(name_upper, name_lower, type) \
        printf("extern void ui_push_%s(%s value);\n", #name_lower, #type);
    LIST_STYLE_STACKS
    printf("\n");
    #undef X

    // Pop
    #define X(name_upper, name_lower, type) \
        printf("extern %s ui_pop_%s(void);\n", #type, #name_lower);
    LIST_STYLE_STACKS
    printf("\n");
    #undef X

    // Next
    #define X(name_upper, name_lower, type) \
        printf("extern void ui_next_%s(%s value);\n", #name_lower, #type);
    LIST_STYLE_STACKS
    printf("\n");
    #undef X


    // Top
    #define X(name_upper, name_lower, type) \
        printf("extern %s ui_top_%s(void);\n", #type, #name_lower);
    LIST_STYLE_STACKS
    #undef X
}

void ui_init(UIStyleStack default_style_stack) {
    ctx = (UIContext) {
        .arena = sp_arena_create(),
        .frame_arenas = {
            sp_arena_create(),
            sp_arena_create(),
        },
        .default_style_stack = default_style_stack,
    };
    sp_arena_tag(ctx.arena, sp_str_lit("ui-state"));
    sp_arena_tag(ctx.frame_arenas[0], sp_str_lit("ui-frame-0"));
    sp_arena_tag(ctx.frame_arenas[1], sp_str_lit("ui-frame-1"));

    ctx.widget_map = sp_hm_new(sp_hm_desc_str(ctx.arena, 4096, UIWidget*));

    // generate_header_functions();
}

void ui_begin(SP_Ivec2 container_size, UIMouse mouse) {
    SP_Arena* arena = ui_get_arena();

    ctx.container = (UIWidget) {
        .id = sp_str_lit("(root_container)"),
        .size = {
            [UI_AXIS_HORIZONTAL] = {
                .kind = UI_SIZE_KIND_PIXELS,
                .value = container_size.x,
                .strictness = 1.0f,
            },
            [UI_AXIS_VERTICAL] = {
                .kind = UI_SIZE_KIND_PIXELS,
                .value = container_size.y,
                .strictness = 1.0f,
            },
        },
        .flags = UI_WIDGET_FLAG_FLOATING,
    };
    ctx.mouse = mouse;

    ui_push_width(ctx.default_style_stack.size[UI_AXIS_HORIZONTAL]);
    ui_push_height(ctx.default_style_stack.size[UI_AXIS_VERTICAL]);
    ui_push_bg(ctx.default_style_stack.bg);
    ui_push_fg(ctx.default_style_stack.fg);
    ui_push_font(ctx.default_style_stack.font);
    ui_push_font_size(ctx.default_style_stack.font_size);
    ui_push_flow(ctx.default_style_stack.flow);
    ui_push_parent(&ctx.container);
    ui_push_fixed_x(0.0f);
    ui_push_fixed_y(0.0f);
}

static void build_fixed_sizes(UIWidget* widget) {
    if (widget == NULL) {
        return;
    }

    SP_Vec2 text_size = sp_v2s(0.0f);
    for (u8 i = 0; i < UI_AXIS_COUNT; i++) {
        if (widget->size[i].kind == UI_SIZE_KIND_TEXT) {
            font_set_size(widget->font, widget->font_size);
            text_size = font_measure_string(widget->font, widget->text);
            break;
        }
    }

    for (u8 i = 0; i < UI_AXIS_COUNT; i++) {
        switch (widget->size[i].kind) {
            case UI_SIZE_KIND_PIXELS:
                widget->computed_size.elements[i] = widget->size[i].value;
                break;
            case UI_SIZE_KIND_TEXT:
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

static SP_Vec2 sum_child_size(UIWidget* widget) {
    SP_Vec2 child_sum = sp_v2s(0.0f);
    for (u8 i = 0; i < UI_AXIS_COUNT; i++) {
        if (widget->size[i].kind == UI_SIZE_KIND_CHILDREN) {
            for (UIWidget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
                if (!(curr_child->flags & UI_WIDGET_FLAG_FLOATING_X << widget->flow)) {
                    child_sum.elements[widget->flow] += curr_child->computed_size.elements[widget->flow];
                    child_sum.elements[!widget->flow] = sp_max(child_sum.elements[!widget->flow], curr_child->computed_size.elements[!widget->flow]);
                }
            }
            break;
        }
    }
    return child_sum;
}

static void build_child_sizes(UIWidget* widget) {
    if (widget == NULL) {
        return;
    }

    // Depth-first
    build_child_sizes(widget->child_first);
    build_child_sizes(widget->next);

    SP_Vec2 child_sum = sum_child_size(widget);
    for (u8 i = 0; i < UI_AXIS_COUNT; i++) {
        if (widget->size[i].kind == UI_SIZE_KIND_CHILDREN) {
            widget->computed_size.elements[i] = child_sum.elements[i];
        }
    }
}

static void build_parent_sizes(UIWidget* widget) {
    if (widget == NULL) {
        return;
    }

    for (u8 i = 0; i < UI_AXIS_COUNT; i++) {
        if (widget->size[i].kind == UI_SIZE_KIND_PARENT && widget->parent != NULL) {
            widget->computed_size.elements[i] = widget->parent->computed_size.elements[i] * widget->size[i].value;
        }
    }

    // Bredth-first
    build_parent_sizes(widget->next);
    build_parent_sizes(widget->child_first);
}

static void solve_size_violations(UIWidget* widget) {
    if (widget == NULL) {
        return;
    }

    // Depth-first
    solve_size_violations(widget->next);
    solve_size_violations(widget->child_first);

    SP_Vec2 child_sum = sum_child_size(widget);
    for (u8 i = 0; i < UI_AXIS_COUNT; i++) {
        f32 violation_amount = child_sum.elements[i] - widget->computed_size.elements[i];

        // Violation
        if (violation_amount > 0.0f) {
            f32 total_budget = 0.0f;
            for (UIWidget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
                total_budget += curr_child->computed_size.elements[i] * (1.0f - curr_child->size[i].strictness);
            }

            if (total_budget < violation_amount) {
                sp_warn("UI sizing violations exceeds budget.");
            }

            for (UIWidget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
                f32 child_budget = curr_child->computed_size.elements[i] * (1.0f - curr_child->size[i].strictness);
                curr_child->computed_size.elements[i] -= child_budget * (violation_amount / total_budget);
                curr_child->computed_size.elements[i] = floorf(curr_child->computed_size.elements[i]);
            }
        }
    }
}

static void build_positions(UIWidget* widget, SP_Vec2 relative_position) {
    if (widget == NULL) {
        return;
    }

    for (UIAxis axis = 0; axis < UI_AXIS_COUNT; axis++) {
        if (!(widget->flags & UI_WIDGET_FLAG_FLOATING_X << axis)) {
            widget->computed_relative_position = relative_position;
            if (widget->parent == NULL) {
                widget->computed_absolute_position.elements[axis] = relative_position.elements[axis];
            } else {
                widget->computed_absolute_position.elements[axis] = widget->parent->computed_absolute_position.elements[axis] + relative_position.elements[axis];
                UIAxis flow = widget->parent->flow;
                if (flow == axis) {
                    relative_position.elements[flow] += widget->computed_size.elements[flow];
                }
            }
        }
    }

    build_positions(widget->next, relative_position);
    build_positions(widget->child_first, sp_v2s(0.0f));
}

void ui_end(void) {
    ctx.current_frame++;

    SP_Arena* arena = ui_get_arena();
    sp_arena_clear(arena);

    build_fixed_sizes(&ctx.container);
    build_child_sizes(&ctx.container);
    build_parent_sizes(&ctx.container);
    solve_size_violations(&ctx.container);
    build_positions(&ctx.container, sp_v2s(0.0f));
}

static void ui_draw_helper(Renderer* renderer, UIWidget* widget) {
    if (widget == NULL) {
        return;
    }

    if (widget->flags & UI_WIDGET_FLAG_DRAW_BACKGROUND) {
        renderer_draw(renderer, (RenderBox) {
                .pos = widget->computed_absolute_position,
                .size = widget->computed_size,
                .color = widget->bg,
            });
    }

    if (widget->flags & UI_WIDGET_FLAG_DRAW_TEXT) {
        font_set_size(widget->font, widget->font_size);
        FontMetrics metrics = font_get_metrics(widget->font);

        // Center text vertically
        SP_Vec2 pos = widget->computed_absolute_position;
        pos.y += widget->computed_size.y / 2.0f;
        pos.y -= (metrics.ascent - metrics.descent) / 2.0f;

        renderer_draw_text(renderer,
                pos,
                widget->text,
                widget->font,
                widget->fg);
    }

    // Bredth-first
    ui_draw_helper(renderer, widget->next);
    ui_draw_helper(renderer, widget->child_first);
}

void ui_draw(Renderer* renderer) {
    ui_draw_helper(renderer, &ctx.container);
}

SP_Arena* ui_get_arena(void) {
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

static UIWidget* widget_from_id(SP_Str* id) {
    UIWidget* widget = sp_hm_get(ctx.widget_map, *id, UIWidget*);

    // TODO: Add free list for no-id widgets and widgets that haven't been
    // touched for two generations.

    // New ID
    if (widget == NULL) {
        widget = sp_arena_push_no_zero(ctx.arena, sizeof(UIWidget));
        sp_hm_insert(ctx.widget_map, *id, widget);
    }
    // Duplicate ID
    else if (widget->last_touched == ctx.current_frame) {
        *id = sp_str_lit("");
        widget = sp_arena_push_no_zero(ctx.arena, sizeof(UIWidget));
    }

    return widget;
}

UIWidget* ui_widget(SP_Str text, UIWidgetFlags flags) {
    SP_Arena* arena = ui_get_arena();
    SP_Str text_copy = sp_str_pushf(arena, "%.*s", text.len, text.data);
    SP_Str id, display_text;
    parse_text(text_copy, &id, &display_text);

    UIWidget* widget = widget_from_id(&id);

    UIWidget* parent = ui_top_parent();
    *widget = (UIWidget) {
        .parent = parent,
        .flags = flags,
        .id = id,
        .text = display_text,
        .last_touched = ctx.current_frame,

        .size = {
            ui_top_width(),
            ui_top_height(),
        },

        // Retain possible state from the last frame
        .computed_absolute_position = widget->computed_absolute_position,
        .computed_size = widget->computed_size,

        .bg = ui_top_bg(),
        .fg = ui_top_fg(),
        .font = ui_top_font(),
        .font_size = ui_top_font_size(),
        .flow = ui_top_flow(),
    };

    if (flags & UI_WIDGET_FLAG_FLOATING_X) {
        widget->computed_absolute_position.x = ui_top_fixed_x();
    }

    if (flags & UI_WIDGET_FLAG_FLOATING_Y) {
        widget->computed_absolute_position.y = ui_top_fixed_y();
    }

    sp_dll_push_back(parent->child_first, parent->child_last, widget);
    return widget;
}

UISignal ui_signal(UIWidget* widget) {
    SP_Vec2 mpos = ctx.mouse.pos;
    f32 left = widget->computed_absolute_position.x;
    f32 right = left + widget->computed_size.x;
    f32 top = widget->computed_absolute_position.y;
    f32 bottom = top + widget->computed_size.y;

    b8 hovered = mpos.x > left && mpos.x < right && mpos.y < bottom && mpos.y > top;
    b8 pressed = hovered && ctx.mouse.buttons[UI_MOUSE_BUTTON_LEFT];
    SP_Vec2 drag = sp_v2s(0.0f);
    if (pressed) {
        drag = ctx.mouse.pos_delta;
    }

    UISignal signal = {
        .hovered = hovered,
        .pressed = pressed,
        .drag = drag,
    };
    return signal;
}

// Push impls
#define X(name_upper, name_lower, type) \
    void ui_push_##name_lower(type value) { \
        SP_Arena* arena = ui_get_arena(); \
        UI##name_upper##Node* node = sp_arena_push_no_zero(arena, sizeof(UI##name_upper##Node)); \
        *node = (UI##name_upper##Node) { \
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
    type ui_pop_##name_lower(void) { \
        sp_assert(ctx.name_lower##_stack->next != NULL, "Too many pops on the %s stack!", #name_lower); \
        type value = ctx.name_lower##_stack->value; \
        sp_sll_stack_pop(ctx.name_lower##_stack); \
        return value; \
    }
LIST_STYLE_STACKS
#undef X

// Push impls
#define X(name_upper, name_lower, type) \
    void ui_next_##name_lower(type value) { \
        SP_Arena* arena = ui_get_arena(); \
        UI##name_upper##Node* node = sp_arena_push_no_zero(arena, sizeof(UI##name_upper##Node)); \
        *node = (UI##name_upper##Node) { \
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
    type ui_top_##name_lower(void) { \
        type value = ctx.name_lower##_stack->value; \
        if (ctx.name_lower##_stack->pop_next) { \
            sp_sll_stack_pop(ctx.name_lower##_stack); \
        } \
        return value; \
    }
LIST_STYLE_STACKS
#undef X
