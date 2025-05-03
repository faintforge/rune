#pragma once

#include "spire.h"
#include "renderer.h"

typedef enum UIWidgetFlags {
    UI_WIDGET_FLAG_NONE            = 0,
    UI_WIDGET_FLAG_DRAW_TEXT       = 1 << 0,
    UI_WIDGET_FLAG_DRAW_BACKGROUND = 1 << 1,
    UI_WIDGET_FLAG_FLOATING_X      = 1 << 2,
    UI_WIDGET_FLAG_FLOATING_Y      = 1 << 3,
    UI_WIDGET_FLAG_FLOATING        = UI_WIDGET_FLAG_FLOATING_X |
                                     UI_WIDGET_FLAG_FLOATING_Y,
    // Widget will consume interaction events and can generate signals.
    UI_WIDGET_FLAG_INTERACTIVE     = 1 << 4,
    UI_WIDGET_FLAG_OVERFLOW_X      = 1 << 5,
    UI_WIDGET_FLAG_OVERFLOW_Y      = 1 << 6,
    UI_WIDGET_FLAG_OVERFLOW        = UI_WIDGET_FLAG_OVERFLOW_X |
                                     UI_WIDGET_FLAG_OVERFLOW_Y,
    UI_WIDGET_FLAG_CLIP            = 1 << 7,
} UIWidgetFlags;

typedef enum UIAxis {
    UI_AXIS_HORIZONTAL,
    UI_AXIS_VERTICAL,
    UI_AXIS_COUNT,
} UIAxis;

typedef enum UISizeKind {
    UI_SIZE_KIND_PIXELS,
    UI_SIZE_KIND_TEXT,
    UI_SIZE_KIND_CHILDREN,
    UI_SIZE_KIND_PARENT,
    UI_SIZE_KIND_COUNT,
} UISizeKind;

typedef struct UISize UISize;
struct UISize {
    UISizeKind kind;
    f32 value;
    f32 strictness;
};

typedef enum UITextAlign {
    UI_TEXT_ALIGN_LEFT,
    UI_TEXT_ALIGN_CENTER,
    UI_TEXT_ALIGN_RIGHT,
} UITextAlign;

typedef struct UIWidget UIWidget;

typedef void (*UIWidgetRenderFunc)(UIWidget* widget, Renderer* renderer, void* userdata);

struct UIWidget {
    UIWidget* parent;

    // Siblings
    UIWidget* next;
    UIWidget* prev;

    // Children
    UIWidget* child_first;
    UIWidget* child_last;

    UIWidget* stack_next;

    UIWidgetFlags flags;
    UISize size[UI_AXIS_COUNT];

    SP_Vec2 computed_relative_position;
    SP_Vec2 computed_absolute_position;
    SP_Vec2 computed_size;

    SP_Str id;
    SP_Str text;
    u32 last_touched;

    UIWidgetRenderFunc render_func;
    void* render_userdata;

    // Style
    SP_Vec4 bg;
    SP_Vec4 fg;
    Font* font;
    u32 font_size;
    UIAxis flow;
    UITextAlign text_align;
};

typedef struct UIStyleStack UIStyleStack;
struct UIStyleStack {
    UISize size[UI_AXIS_COUNT];
    SP_Vec4 bg;
    SP_Vec4 fg;
    Font* font;
    u32 font_size;
    UIAxis flow;
    UITextAlign text_align;
};

typedef enum UIMouseButton {
    UI_MOUSE_BUTTON_LEFT,
    UI_MOUSE_BUTTON_MIDDLE,
    UI_MOUSE_BUTTON_RIGHT,
    UI_MOUSE_BUTTON_COUNT,
} UIMouseButton;

typedef struct UIMouse UIMouse;
struct UIMouse {
    struct {
        b8 pressed;
        b8 clicked;
    } buttons[UI_MOUSE_BUTTON_COUNT];
    SP_Vec2 pos;
    SP_Vec2 pos_delta;
    f32 scroll;
};

typedef struct UISignal UISignal;
struct UISignal {
    b8 hovered;
    b8 pressed;
    b8 clicked;
    b8 focused;
    SP_Vec2 drag;
};

extern void ui_init(UIStyleStack default_style_stack);
extern void ui_begin(SP_Ivec2 container_size, UIMouse mouse);
extern void ui_end(void);
extern void ui_draw(Renderer* renderer);

extern SP_Arena* ui_get_arena(void);
extern UIWidget* ui_widget(SP_Str text, UIWidgetFlags flags);
extern void ui_widget_equip_render_func(UIWidget* widget, UIWidgetRenderFunc func, void* userdata);

extern UISignal ui_signal(UIWidget* widget);

#define UI_SIZE_PIXELS(VALUE, STRICTNESS) \
    ((UISize) { \
        .kind = UI_SIZE_KIND_PIXELS, \
        .value = VALUE, \
        .strictness = STRICTNESS \
    })
#define UI_SIZE_TEXT(STRICTNESS) \
    ((UISize) { \
        .kind = UI_SIZE_KIND_TEXT, \
        .strictness = STRICTNESS \
    })
#define UI_SIZE_CHILDREN(STRICTNESS) \
    ((UISize) { \
        .kind = UI_SIZE_KIND_CHILDREN, \
        .strictness = STRICTNESS \
    })
#define UI_SIZE_PARENT(VALUE, STRICTNESS) \
    ((UISize) { \
        .kind = UI_SIZE_KIND_PARENT, \
        .value = VALUE, \
        .strictness = STRICTNESS \
    })

// Stack operations
extern void ui_push_width(UISize value);
extern void ui_push_height(UISize value);
extern void ui_push_bg(SP_Vec4 value);
extern void ui_push_fg(SP_Vec4 value);
extern void ui_push_font(Font* value);
extern void ui_push_font_size(u32 value);
extern void ui_push_flow(UIAxis value);
extern void ui_push_parent(UIWidget* value);
extern void ui_push_fixed_x(f32 value);
extern void ui_push_fixed_y(f32 value);
extern void ui_push_text_align(UITextAlign value);

extern UISize ui_pop_width(void);
extern UISize ui_pop_height(void);
extern SP_Vec4 ui_pop_bg(void);
extern SP_Vec4 ui_pop_fg(void);
extern Font* ui_pop_font(void);
extern u32 ui_pop_font_size(void);
extern UIAxis ui_pop_flow(void);
extern UIWidget* ui_pop_parent(void);
extern f32 ui_pop_fixed_x(void);
extern f32 ui_pop_fixed_y(void);
extern UITextAlign ui_pop_text_align(void);

extern void ui_next_width(UISize value);
extern void ui_next_height(UISize value);
extern void ui_next_bg(SP_Vec4 value);
extern void ui_next_fg(SP_Vec4 value);
extern void ui_next_font(Font* value);
extern void ui_next_font_size(u32 value);
extern void ui_next_flow(UIAxis value);
extern void ui_next_parent(UIWidget* value);
extern void ui_next_fixed_x(f32 value);
extern void ui_next_fixed_y(f32 value);
extern void ui_next_text_align(UITextAlign value);

extern UISize ui_top_width(void);
extern UISize ui_top_height(void);
extern SP_Vec4 ui_top_bg(void);
extern SP_Vec4 ui_top_fg(void);
extern Font* ui_top_font(void);
extern u32 ui_top_font_size(void);
extern UIAxis ui_top_flow(void);
extern UIWidget* ui_top_parent(void);
extern f32 ui_top_fixed_x(void);
extern f32 ui_top_fixed_y(void);
extern UITextAlign ui_top_text_align(void);
