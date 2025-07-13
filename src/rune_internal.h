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
    X(TextAlign, text_align, RNE_TextAlign) \
    X(CornerRadius, corner_radius, SP_Vec4) \
    X(Padding, padding, SP_Vec4) \
    X(Offset, offset, RNE_Offset)

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

#define WIDGET_MAP_COUNT 1

typedef struct RNE_WidgetMap RNE_WidgetMap;
struct RNE_WidgetMap {
    SP_Arena* arena;
    RNE_Widget widgets[WIDGET_MAP_COUNT];
    RNE_Widget* free_stack;
    RNE_Widget* no_id_stack;
};

#define X(name_upper, name_lower, type) RNE_##name_upper##Node* name_lower##_stack;
typedef struct RNE_Context RNE_Context;
struct RNE_Context {
    SP_Arena* arena;
    SP_Arena* frame_arenas[2];
    RNE_Widget container;
    u64 current_frame;

    RNE_WidgetMap widget_map;
    RNE_Widget* widget_no_id_stack;
    RNE_Widget* widget_free_stack;
    u64 current_hash;

    RNE_Widget* focused_widget;
    RNE_Widget* active_widget;
    RNE_InternalMouse mouse;

    RNE_TextMeasureFunc text_measure;

    // Styles
    RNE_StyleStack default_style_stack;
    LIST_STYLE_STACKS
};
#undef X

// Defined in rune.c
extern RNE_Context ctx;

extern RNE_WidgetMap rne_widget_map_init(SP_Arena* arena);
extern RNE_Widget* rne_widget_map_request(RNE_WidgetMap* map, u64 hash, SP_Str id);
extern void rne_widget_map_cleanup(RNE_WidgetMap* map);
