#pragma once

#include "spire.h"
#include "renderer.h"

typedef enum UIWidgetFlags {
    UI_WIDGET_FLAG_NONE            = 0,
    UI_WIDGET_FLAG_DRAW_TEXT       = 1 << 0,
    UI_WIDGET_FLAG_DRAW_BACKGROUND = 1 << 1,
    UI_WIDGET_FLAG_DRAW_FLOATING_X = 1 << 2,
    UI_WIDGET_FLAG_DRAW_FLOATING_Y = 1 << 3,
    UI_WIDGET_FLAG_DRAW_FLOATING   = UI_WIDGET_FLAG_DRAW_FLOATING_X |
                                     UI_WIDGET_FLAG_DRAW_FLOATING_Y
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
    UI_SIZE_KIND_PERCENT,
    UI_SIZE_KIND_COUNT,
} UISizeKind;

typedef struct UISize UISize;
struct UISize {
    UISizeKind kind;
    f32 value;
    f32 strictness;
};

typedef struct UIWidget UIWidget;
struct UIWidget {
    UIWidget* parent;

    // Siblings
    UIWidget* next;
    UIWidget* prev;

    // Children
    UIWidget* child_first;
    UIWidget* child_last;

    UIWidgetFlags flags;
    UISize size[UI_AXIS_COUNT];

    SP_Vec2 computed_relative_position;
    SP_Vec2 computed_absolute_position;
    SP_Vec2 computed_size;

    SP_Str id;
    SP_Str text;

    // Style
    SP_Vec4 bg;
    SP_Vec4 fg;
    Font* font;
    u32 font_size;
    UIAxis flow;
};

typedef struct UIStyleStack UIStyleStack;
struct UIStyleStack {
    UISize size[UI_AXIS_COUNT];
    SP_Vec4 bg;
    SP_Vec4 fg;
    Font* font;
    u32 font_size;
    UIAxis flow;
};

extern void ui_init(UIStyleStack default_style_stack);
extern void ui_begin(SP_Ivec2 container_size);
extern void ui_end(void);
extern void ui_draw(Renderer* renderer);

extern SP_Arena* ui_get_arena(void);
extern UIWidget* ui_widget(SP_Str text, UIWidgetFlags flags);

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
