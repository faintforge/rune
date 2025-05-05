#pragma once

#include "spire.h"
#include "renderer.h"

typedef enum RNE_WidgetFlags {
    RNE_WIDGET_FLAG_NONE            = 0,
    RNE_WIDGET_FLAG_DRAW_TEXT       = 1 << 0,
    RNE_WIDGET_FLAG_DRAW_BACKGROUND = 1 << 1,
    RNE_WIDGET_FLAG_FLOATING_X      = 1 << 2,
    RNE_WIDGET_FLAG_FLOATING_Y      = 1 << 3,
    RNE_WIDGET_FLAG_FLOATING        = RNE_WIDGET_FLAG_FLOATING_X |
                                      RNE_WIDGET_FLAG_FLOATING_Y,
    // Widget will consume interaction events and can generate signals.
    RNE_WIDGET_FLAG_INTERACTIVE     = 1 << 4,
    RNE_WIDGET_FLAG_OVERFLOW_X      = 1 << 5,
    RNE_WIDGET_FLAG_OVERFLOW_Y      = 1 << 6,
    RNE_WIDGET_FLAG_OVERFLOW        = RNE_WIDGET_FLAG_OVERFLOW_X |
                                      RNE_WIDGET_FLAG_OVERFLOW_Y,
    RNE_WIDGET_FLAG_CLIP            = 1 << 7,
    RNE_WIDGET_FLAG_VIEW_SCROLL     = 1 << 8,
} RNE_WidgetFlags;

typedef enum RNE_Axis {
    RNE_AXIS_HORIZONTAL,
    RNE_AXIS_VERTICAL,
    RNE_AXIS_COUNT,
} RNE_Axis;

typedef enum RNE_SizeKind {
    RNE_SIZE_KIND_PIXELS,
    RNE_SIZE_KIND_TEXT,
    RNE_SIZE_KIND_CHILDREN,
    RNE_SIZE_KIND_PARENT,
    RNE_SIZE_KIND_COUNT,
} RNE_SizeKind;

typedef struct RNE_Size RNE_Size;
struct RNE_Size {
    RNE_SizeKind kind;
    f32 value;
    f32 strictness;
};

typedef enum RNE_TextAlign {
    RNE_TEXT_ALIGN_LEFT,
    RNE_TEXT_ALIGN_CENTER,
    RNE_TEXT_ALIGN_RIGHT,
} RNE_TextAlign;

typedef struct RNE_Widget RNE_Widget;

typedef void (*RNE_WidgetRenderFunc)(RNE_Widget* widget, Renderer* renderer, void* userdata);

struct RNE_Widget {
    RNE_Widget* parent;

    // Siblings
    RNE_Widget* next;
    RNE_Widget* prev;

    // Children
    RNE_Widget* child_first;
    RNE_Widget* child_last;
    SP_Vec2 child_size_sum;

    RNE_Widget* stack_next;

    RNE_WidgetFlags flags;
    RNE_Size size[RNE_AXIS_COUNT];

    SP_Vec2 computed_relative_position;
    SP_Vec2 computed_absolute_position;
    SP_Vec2 computed_size;
    SP_Vec2 view_offset;

    SP_Str id;
    SP_Str text;
    u32 last_touched;

    RNE_WidgetRenderFunc render_func;
    void* render_userdata;

    // Style
    SP_Vec4 bg;
    SP_Vec4 fg;
    Font* font;
    u32 font_size;
    RNE_Axis flow;
    RNE_TextAlign text_align;
};

typedef struct RNE_StyleStack RNE_StyleStack;
struct RNE_StyleStack {
    RNE_Size size[RNE_AXIS_COUNT];
    SP_Vec4 bg;
    SP_Vec4 fg;
    Font* font;
    u32 font_size;
    RNE_Axis flow;
    RNE_TextAlign text_align;
};

typedef enum RNE_MouseButton {
    RNE_MOUSE_BUTTON_LEFT,
    RNE_MOUSE_BUTTON_MIDDLE,
    RNE_MOUSE_BUTTON_RIGHT,
    RNE_MOUSE_BUTTON_COUNT,
} RNE_MouseButton;

typedef struct RNE_Mouse RNE_Mouse;
struct RNE_Mouse {
    struct {
        b8 pressed;
        b8 clicked;
    } buttons[RNE_MOUSE_BUTTON_COUNT];
    SP_Vec2 pos;
    SP_Vec2 pos_delta;
    f32 scroll;
};

typedef struct RNE_Signal RNE_Signal;
struct RNE_Signal {
    b8 hovered;
    b8 pressed;
    b8 clicked;
    b8 focused;
    SP_Vec2 drag;
    f32 scroll;
};

extern void rne_init(RNE_StyleStack default_style_stack);
extern void rne_begin(SP_Ivec2 container_size, RNE_Mouse mouse);
extern void rne_end(void);
extern void rne_draw(Renderer* renderer);

extern SP_Arena* rne_get_arena(void);
extern RNE_Widget* rne_widget(SP_Str text, RNE_WidgetFlags flags);
extern void rne_widget_equip_render_func(RNE_Widget* widget, RNE_WidgetRenderFunc func, void* userdata);

extern RNE_Signal rne_signal(RNE_Widget* widget);

#define RNE_SIZE_PIXELS(VALUE, STRICTNESS) \
    ((RNE_Size) { \
        .kind = RNE_SIZE_KIND_PIXELS, \
        .value = VALUE, \
        .strictness = STRICTNESS \
    })
#define RNE_SIZE_TEXT(STRICTNESS) \
    ((RNE_Size) { \
        .kind = RNE_SIZE_KIND_TEXT, \
        .strictness = STRICTNESS \
    })
#define RNE_SIZE_CHILDREN(STRICTNESS) \
    ((RNE_Size) { \
        .kind = RNE_SIZE_KIND_CHILDREN, \
        .strictness = STRICTNESS \
    })
#define RNE_SIZE_PARENT(VALUE, STRICTNESS) \
    ((RNE_Size) { \
        .kind = RNE_SIZE_KIND_PARENT, \
        .value = VALUE, \
        .strictness = STRICTNESS \
    })

// Stack operations
extern void rne_push_width(RNE_Size value);
extern void rne_push_height(RNE_Size value);
extern void rne_push_bg(SP_Vec4 value);
extern void rne_push_fg(SP_Vec4 value);
extern void rne_push_font(Font* value);
extern void rne_push_font_size(u32 value);
extern void rne_push_flow(RNE_Axis value);
extern void rne_push_parent(RNE_Widget* value);
extern void rne_push_fixed_x(f32 value);
extern void rne_push_fixed_y(f32 value);
extern void rne_push_text_align(RNE_TextAlign value);

extern RNE_Size rne_pop_width(void);
extern RNE_Size rne_pop_height(void);
extern SP_Vec4 rne_pop_bg(void);
extern SP_Vec4 rne_pop_fg(void);
extern Font* rne_pop_font(void);
extern u32 rne_pop_font_size(void);
extern RNE_Axis rne_pop_flow(void);
extern RNE_Widget* rne_pop_parent(void);
extern f32 rne_pop_fixed_x(void);
extern f32 rne_pop_fixed_y(void);
extern RNE_TextAlign rne_pop_text_align(void);

extern void rne_next_width(RNE_Size value);
extern void rne_next_height(RNE_Size value);
extern void rne_next_bg(SP_Vec4 value);
extern void rne_next_fg(SP_Vec4 value);
extern void rne_next_font(Font* value);
extern void rne_next_font_size(u32 value);
extern void rne_next_flow(RNE_Axis value);
extern void rne_next_parent(RNE_Widget* value);
extern void rne_next_fixed_x(f32 value);
extern void rne_next_fixed_y(f32 value);
extern void rne_next_text_align(RNE_TextAlign value);

extern RNE_Size rne_top_width(void);
extern RNE_Size rne_top_height(void);
extern SP_Vec4 rne_top_bg(void);
extern SP_Vec4 rne_top_fg(void);
extern Font* rne_top_font(void);
extern u32 rne_top_font_size(void);
extern RNE_Axis rne_top_flow(void);
extern RNE_Widget* rne_top_parent(void);
extern f32 rne_top_fixed_x(void);
extern f32 rne_top_fixed_y(void);
extern RNE_TextAlign rne_top_text_align(void);
