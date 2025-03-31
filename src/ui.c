#include "ui.h"
#include "font.h"
#include "renderer.h"
#include "spire.h"

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
    SP_Arena* arenas[2];
    UIWidget container;
    u64 current_frame;

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
        .arenas = {
            sp_arena_create(),
            sp_arena_create(),
        },
        .default_style_stack = default_style_stack,
    };

    // generate_header_functions();
}

void ui_begin(SP_Ivec2 container_size) {
    SP_Arena* arena = ui_get_arena();
    sp_arena_clear(arena);

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
        .flags = UI_WIDGET_FLAG_DRAW_FLOATING,
    };

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

    build_fixed_sizes(widget->next);
    build_fixed_sizes(widget->child_first);
}

static void build_child_sizes(UIWidget* widget) {
    if (widget == NULL) {
        return;
    }

    build_child_sizes(widget->child_first);
    build_child_sizes(widget->next);

    SP_Vec2 child_sum = sp_v2s(0.0f);
    for (u8 i = 0; i < UI_AXIS_COUNT; i++) {
        if (widget->size[i].kind == UI_SIZE_KIND_CHILDREN) {
            for (UIWidget* curr_child = widget->child_first; curr_child != NULL; curr_child = curr_child->next) {
                if (!(curr_child->flags & UI_WIDGET_FLAG_DRAW_FLOATING_X << widget->flow)) {
                    child_sum.elements[widget->flow] += curr_child->computed_size.elements[widget->flow];
                    child_sum.elements[!widget->flow] = sp_max(child_sum.elements[!widget->flow], curr_child->computed_size.elements[!widget->flow]);
                }
            }
            break;
        }
    }

    for (u8 i = 0; i < UI_AXIS_COUNT; i++) {
        if (widget->size[i].kind == UI_SIZE_KIND_CHILDREN) {
            widget->computed_size.elements[i] = child_sum.elements[i];
        }
    }
}

static void build_positions(UIWidget* widget, SP_Vec2 relative_position) {
    if (widget == NULL) {
        return;
    }

    for (UIAxis axis = 0; axis < UI_AXIS_COUNT; axis++) {
        if (!(widget->flags & UI_WIDGET_FLAG_DRAW_FLOATING_X << axis)) {
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

    // TODO: Calculate layout
    build_fixed_sizes(&ctx.container);
    build_child_sizes(&ctx.container);
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
    return ctx.arenas[ctx.current_frame % sp_arrlen(ctx.arenas)];
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

UIWidget* ui_widget(SP_Str text, UIWidgetFlags flags) {
    SP_Arena* arena = ui_get_arena();
    UIWidget* widget = sp_arena_push_no_zero(arena, sizeof(UIWidget));

    SP_Str text_copy = sp_str_pushf(arena, "%.*s", text.len, text.data);
    SP_Str id, display_text;
    parse_text(text_copy, &id, &display_text);

    UIWidget* parent = ui_top_parent();
    *widget = (UIWidget) {
        .parent = parent,
        .flags = flags,
        .id = id,
        .text = display_text,

        .size = {
            ui_top_width(),
            ui_top_height(),
        },
        .computed_absolute_position = sp_v2(ui_top_fixed_x(), ui_top_fixed_y()),

        .bg = ui_top_bg(),
        .fg = ui_top_fg(),
        .font = ui_top_font(),
        .font_size = ui_top_font_size(),
        .flow = ui_top_flow(),
    };
    sp_dll_push_back(parent->child_first, parent->child_last, widget);
    // sp_debug("%.*s", parent->id.len, parent->id.data);
    return widget;
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
