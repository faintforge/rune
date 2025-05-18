#pragma once

#include "spire.h"

typedef union RNE_Handle RNE_Handle;
union RNE_Handle {
    void* ptr;
    u64 id;
};

// -- Drawing ------------------------------------------------------------------

typedef enum RNE_DrawCmdType {
    RNE_DRAW_CMD_TYPE_LINE,
    RNE_DRAW_CMD_TYPE_ARC,
    RNE_DRAW_CMD_TYPE_CIRCLE,
    RNE_DRAW_CMD_TYPE_RECT,
    RNE_DRAW_CMD_TYPE_IMAGE,
    RNE_DRAW_CMD_TYPE_TEXT,
    RNE_DRAW_CMD_TYPE_SCISSOR,
} RNE_DrawCmdType;

typedef struct RNE_DrawLine RNE_DrawLine;
struct RNE_DrawLine {
    SP_Vec2 a;
    SP_Vec2 b;
    SP_Color color;
    f32 thickness;
};

typedef struct RNE_DrawArc RNE_DrawArc;
struct RNE_DrawArc {
    SP_Vec2 pos;
    f32 radius;
    f32 start_angle;
    f32 end_angle;
    SP_Color color;
    u32 segments;
};

typedef struct RNE_DrawCircle RNE_DrawCircle;
struct RNE_DrawCircle {
    SP_Vec2 pos;
    f32 radius;
    SP_Color color;
    u32 segments;
};

typedef struct RNE_DrawRect RNE_DrawRect;
struct RNE_DrawRect {
    SP_Vec2 pos;
    SP_Vec2 size;
    SP_Vec4 corner_radius;
    u32 corner_segments;
    SP_Color color;
};

typedef struct RNE_DrawText RNE_DrawText;
struct RNE_DrawText {
    SP_Str text;
    SP_Vec2 pos;
    SP_Color color;
    RNE_Handle font_handle;
    f32 font_size;
};

typedef struct RNE_DrawImage RNE_DrawImage;
struct RNE_DrawImage {
    RNE_Handle texture_handle;
    SP_Vec2 pos;
    SP_Vec2 size;
    SP_Color color;
    // [0] = Top left
    // [1] = Bottom right
    SP_Vec2 uv[2];
};

typedef struct RNE_DrawScissor RNE_DrawScissor;
struct RNE_DrawScissor {
    SP_Vec2 pos;
    SP_Vec2 size;
};

typedef struct RNE_DrawCmd RNE_DrawCmd;
struct RNE_DrawCmd {
    RNE_DrawCmdType type;
    f32 thickness;
    b8 filled;
    b8 closed;
    union {
        RNE_DrawLine line;
        RNE_DrawArc arc;
        RNE_DrawCircle circle;
        RNE_DrawRect rect;
        RNE_DrawText text;
        RNE_DrawImage image;
        RNE_DrawScissor scissor;
    } data;

    RNE_DrawCmd* next;
    RNE_DrawCmd* prev;
};

typedef struct RNE_DrawCmdBuffer RNE_DrawCmdBuffer;

extern RNE_DrawCmdBuffer* rne_draw_buffer_begin(SP_Arena* arena);
extern void rne_draw_buffer_push(RNE_DrawCmdBuffer* buffer, RNE_DrawCmd cmd);

extern void rne_draw_line(RNE_DrawCmdBuffer* buffer, RNE_DrawLine line);
extern void rne_draw_arc_filled(RNE_DrawCmdBuffer* buffer, RNE_DrawArc arc);
extern void rne_draw_arc_stroke(RNE_DrawCmdBuffer* buffer, RNE_DrawArc arc, f32 thickness);
extern void rne_draw_circle_filled(RNE_DrawCmdBuffer* buffer, RNE_DrawCircle circle);
extern void rne_draw_circle_stroke(RNE_DrawCmdBuffer* buffer, RNE_DrawCircle circle, f32 thickness);
extern void rne_draw_rect_filled(RNE_DrawCmdBuffer* buffer, RNE_DrawRect rect);
extern void rne_draw_rect_stroke(RNE_DrawCmdBuffer* buffer, RNE_DrawRect rect, f32 thickness);
extern void rne_draw_image(RNE_DrawCmdBuffer* buffer, RNE_DrawImage image);
extern void rne_draw_text(RNE_DrawCmdBuffer* buffer, RNE_DrawText text);
extern void rne_draw_scissor(RNE_DrawCmdBuffer* buffer, RNE_DrawScissor scissor);

// -- Widget -------------------------------------------------------------------

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

typedef void (*RNE_WidgetRenderFunc)(RNE_DrawCmdBuffer* buffer, RNE_Widget* widget, void* userdata);

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
    // Offset from outer size to inner size. Used to know where children and
    // text should be placed.
    SP_Vec2 computed_inner_position;
    // Size parent uses (except for sizing violations)
    SP_Vec2 computed_outer_size;
    // Size children use
    SP_Vec2 computed_inner_size;
    SP_Vec2 view_offset;

    SP_Str id;
    SP_Str text;
    u32 last_touched;

    RNE_WidgetRenderFunc render_func;
    void* render_userdata;

    // Style
    SP_Color bg;
    SP_Color fg;
    RNE_Handle font;
    f32 font_size;
    RNE_Axis flow;
    RNE_TextAlign text_align;
    SP_Vec4 corner_radius;
    SP_Vec4 padding;
};

typedef struct RNE_StyleStack RNE_StyleStack;
struct RNE_StyleStack {
    RNE_Size size[RNE_AXIS_COUNT];
    SP_Color bg;
    SP_Color fg;
    RNE_Handle font;
    f32 font_size;
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
    b8 buttons[RNE_MOUSE_BUTTON_COUNT];
    SP_Vec2 pos;
    f32 scroll;
};

typedef struct RNE_Signal RNE_Signal;
struct RNE_Signal {
    b8 hovered;
    b8 pressed;
    b8 just_pressed;
    b8 just_released;
    b8 focused;
    SP_Vec2 drag;
    f32 scroll;
};

typedef SP_Vec2 (*RNE_TextMeasureFunc)(RNE_Handle font, SP_Str text, f32 size);

extern void rne_init(RNE_StyleStack default_style_stack, RNE_TextMeasureFunc text_measure_func);
extern void rne_begin(SP_Ivec2 container_size, RNE_Mouse mouse);
extern void rne_end(void);
extern RNE_DrawCmdBuffer* rne_draw(SP_Arena* arena);

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
extern void rne_push_bg(SP_Color value);
extern void rne_push_fg(SP_Color value);
extern void rne_push_font(RNE_Handle value);
extern void rne_push_font_size(f32 value);
extern void rne_push_flow(RNE_Axis value);
extern void rne_push_parent(RNE_Widget* value);
extern void rne_push_fixed_x(f32 value);
extern void rne_push_fixed_y(f32 value);
extern void rne_push_text_align(RNE_TextAlign value);
extern void rne_push_corner_radius(SP_Vec4 value);
extern void rne_push_padding(SP_Vec4 value);

extern RNE_Size rne_pop_width(void);
extern RNE_Size rne_pop_height(void);
extern SP_Color rne_pop_bg(void);
extern SP_Color rne_pop_fg(void);
extern RNE_Handle rne_pop_font(void);
extern f32 rne_pop_font_size(void);
extern RNE_Axis rne_pop_flow(void);
extern RNE_Widget* rne_pop_parent(void);
extern f32 rne_pop_fixed_x(void);
extern f32 rne_pop_fixed_y(void);
extern RNE_TextAlign rne_pop_text_align(void);
extern SP_Vec4 rne_pop_corner_radius(void);
extern SP_Vec4 rne_pop_padding(void);

extern void rne_next_width(RNE_Size value);
extern void rne_next_height(RNE_Size value);
extern void rne_next_bg(SP_Color value);
extern void rne_next_fg(SP_Color value);
extern void rne_next_font(RNE_Handle value);
extern void rne_next_font_size(f32 value);
extern void rne_next_flow(RNE_Axis value);
extern void rne_next_parent(RNE_Widget* value);
extern void rne_next_fixed_x(f32 value);
extern void rne_next_fixed_y(f32 value);
extern void rne_next_text_align(RNE_TextAlign value);
extern void rne_next_corner_radius(SP_Vec4 value);
extern void rne_next_padding(SP_Vec4 value);

extern RNE_Size rne_top_width(void);
extern RNE_Size rne_top_height(void);
extern SP_Color rne_top_bg(void);
extern SP_Color rne_top_fg(void);
extern RNE_Handle rne_top_font(void);
extern f32 rne_top_font_size(void);
extern RNE_Axis rne_top_flow(void);
extern RNE_Widget* rne_top_parent(void);
extern f32 rne_top_fixed_x(void);
extern f32 rne_top_fixed_y(void);
extern RNE_TextAlign rne_top_text_align(void);
extern SP_Vec4 rne_top_corner_radius(void);
extern SP_Vec4 rne_top_padding(void);
