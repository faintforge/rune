#pragma once

#include "rune/rune.h"

// Style stacks
// #define X(name_upper, name_lower, type)
#define LIST_STYLE_STACKS \
    X(Width, width, RNE_Size) \
    X(Height, height, RNE_Size) \
    X(Bg, bg, SP_Color) \
    X(Fg, fg, SP_Color) \
    X(Font, font, RNE_Handle) \
    X(FontSize, font_size, f32) \
    X(Flow, flow, RNE_Axis) \
    X(Parent, parent, RNE_Widget*) \
    X(FixedX, fixed_x, f32) \
    X(FixedY, fixed_y, f32) \
    X(TextAlign, text_align, RNE_TextAlign) \
    X(CornerRadius, corner_radius, SP_Vec4) \
    X(Padding, padding, SP_Vec4)

#define X(name_upper, name_lower, type) \
    typedef struct RNE_##name_upper##Node RNE_##name_upper##Node; \
    struct RNE_##name_upper##Node { \
        RNE_##name_upper##Node* next; \
        type value; \
        b8 pop_next; \
    };
LIST_STYLE_STACKS
#undef X

typedef struct RNE_InternalMouse RNE_InternalMouse;
struct RNE_InternalMouse {
    struct {
        b8 first_frame_pressed;
        b8 first_frame_released;
        b8 down;
    } buttons[RNE_MOUSE_BUTTON_COUNT];
    SP_Vec2 pos;
    SP_Vec2 pos_delta;
    f32 scroll;
};

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
    RNE_InternalMouse mouse;

    RNE_TextMeasureFunc text_measure;

    // Styles
    RNE_StyleStack default_style_stack;
    LIST_STYLE_STACKS
};
#undef X

// Defined in rune.c
extern RNE_Context ctx;

struct RNE_DrawCmdBuffer {
    SP_Arena* arena;
    RNE_DrawCmd* first;
    RNE_DrawCmd* last;
};
