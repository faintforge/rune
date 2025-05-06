#include "new_renderer.h"
#include "spire.h"
#include "renderer.h"
#include "font.h"
#include "rune.h"

#include <assert.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

SP_Vec2 measure(RNE_Handle font, SP_Str text, f32 height) {
    Font* f = font.ptr;
    font_set_size(f, height);
    return font_measure_string(f, text);
}

RNE_Glyph query(RNE_Handle font, u32 codepoint, f32 height) {
    Font* f = font.ptr;
    font_set_size(f, height);
    Glyph g = font_get_glyph(f, codepoint);
    return (RNE_Glyph) {
        .size = g.size,
        .offset = g.offset,
        .advance = g.advance,
        .uv = { g.uv[0], g.uv[1] }
    };
}

static RNE_Mouse mouse = {0};
static void reset_mouse(void) {
    mouse.pos_delta = sp_v2s(0.0f);
    mouse.scroll = 0.0f;
    for (RNE_MouseButton btn = 0; btn < RNE_MOUSE_BUTTON_COUNT; btn++) {
        mouse.buttons[btn].clicked = false;
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    switch (button) {
        case GLFW_MOUSE_BUTTON_1:
            mouse.buttons[RNE_MOUSE_BUTTON_LEFT].pressed = action;
            mouse.buttons[RNE_MOUSE_BUTTON_LEFT].clicked = action;
            break;
        case GLFW_MOUSE_BUTTON_2:
            mouse.buttons[RNE_MOUSE_BUTTON_RIGHT].pressed = action;
            mouse.buttons[RNE_MOUSE_BUTTON_RIGHT].clicked = action;
            break;
        case GLFW_MOUSE_BUTTON_3:
            mouse.buttons[RNE_MOUSE_BUTTON_MIDDLE].pressed = action;
            mouse.buttons[RNE_MOUSE_BUTTON_MIDDLE].clicked = action;
            break;
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    SP_Vec2 pos = sp_v2(xpos, ypos);
    SP_Vec2 delta = sp_v2_sub(pos, mouse.pos);
    mouse.pos = pos;
    mouse.pos_delta = sp_v2_add(mouse.pos_delta, delta);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    mouse.scroll += yoffset;
}


static void column_start(void) {
    // rne_next_width(RNE_SIZE_CHILDREN(1.0f));
    // rne_next_height(RNE_SIZE_CHILDREN(1.0f));
    rne_next_flow(RNE_AXIS_VERTICAL);
    RNE_Widget* column = rne_widget(sp_str_lit(""), RNE_WIDGET_FLAG_NONE);
    rne_push_parent(column);
}

static void column_end(void) {
    rne_pop_parent();
}

static void row_start(void) {
    // rne_next_width(RNE_SIZE_CHILDREN(1.0f));
    // rne_next_height(RNE_SIZE_CHILDREN(1.0f));
    rne_next_flow(RNE_AXIS_HORIZONTAL);
    RNE_Widget* row = rne_widget(sp_str_lit(""), RNE_WIDGET_FLAG_NONE);
    rne_push_parent(row);
}

static void row_end(void) {
    rne_pop_parent();
}

#define DEFER(BEGIN, END) for (b8 _i_ = (BEGIN, false); !_i_; _i_ = (END, true))
#define column() DEFER(column_start(), column_end())
#define row() DEFER(row_start(), row_end())

static RNE_Widget* spacer(RNE_Size size) {
    RNE_Axis flow = rne_top_parent()->flow;
    switch (flow) {
        case RNE_AXIS_HORIZONTAL:
            rne_next_width(size);
            rne_next_height(RNE_SIZE_PIXELS(0.0f, 0.0f));
            break;
        case RNE_AXIS_VERTICAL:
            rne_next_width(RNE_SIZE_PIXELS(0.0f, 0.0f));
            rne_next_height(size);
            break;
        case RNE_AXIS_COUNT:
            break;
    }
    return rne_widget(sp_str_lit(""), RNE_WIDGET_FLAG_NONE);
}

static RNE_Widget* text(SP_Str text) {
    rne_next_width(RNE_SIZE_TEXT(1.0));
    rne_next_height(RNE_SIZE_TEXT(1.0));
    return rne_widget(text, RNE_WIDGET_FLAG_DRAW_TEXT);
}

static void checkbox_render(RNE_DrawCmdBuffer* buffer, RNE_Widget* widget, void* userdata) {
    b8 value = *(b8*) userdata;
    if (!value) {
        return;
    }

    const f32 fill_amount = 0.6f;

    SP_Vec2 size = widget->computed_size;
    size = sp_v2_muls(size, fill_amount);

    SP_Vec2 pos = widget->computed_absolute_position;
    pos = sp_v2_add(pos, sp_v2_divs(sp_v2_sub(widget->computed_size, size), 2.0f));

    // rne_draw_rect_filled(buffer, (RNE_DrawRect) {
    //         .pos = pos,
    //         .size = size,
    //         .color = sp_color_rgba_f(0.75f, 0.75f, 0.75f, 1.0f),
    //     });

    f32 radius = size.x / 2.0f;
    rne_draw_circle_filled(buffer, (RNE_DrawCircle) {
            .pos = sp_v2_adds(pos, radius),
            .radius = radius,
            .segments = 16,
            .color = sp_color_rgba_f(0.75f, 0.75f, 0.75f, 1.0f),
        });
}

static RNE_Widget* checkbox(SP_Str id, b8* value) {
    rne_next_width(RNE_SIZE_PIXELS(32.0f, 1.0f));
    rne_next_height(RNE_SIZE_PIXELS(32.0f, 1.0f));
    rne_next_bg(sp_color_rgba_f(0.2f, 0.2f, 0.2f, 1.0f));
    RNE_Widget* checkbox = rne_widget(id, RNE_WIDGET_FLAG_DRAW_BACKGROUND | RNE_WIDGET_FLAG_INTERACTIVE);
    rne_widget_equip_render_func(checkbox, checkbox_render, value);

    RNE_Signal signal = rne_signal(checkbox);
    if (signal.clicked) {
        *value = !*value;
    }

    return checkbox;
}

typedef struct Rect Rect;
struct Rect {
    SP_Vec2 pos;
    SP_Vec2 size;
    SP_Color color;
};

typedef union SliderData SliderData;
union SliderData {
    struct {
        Rect bar;
        Rect fill;
    };
    Rect r[2];
};

static void slider_render(RNE_DrawCmdBuffer* buffer, RNE_Widget* widget, void* userdata) {
    SliderData* data = userdata;
    for (u8 i = 0; i < sp_arrlen(data->r); i++) {
        rne_draw_rect_filled(buffer, (RNE_DrawRect) {
                .pos = data->r[i].pos,
                .size = data->r[i].size,
                .color = data->r[i].color,
            });
    }
}

static RNE_Widget* slider(SP_Str id, f32* value, f32 min, f32 max) {
    SP_Arena* arena = rne_get_arena();

    rne_next_width(RNE_SIZE_PIXELS(128.0f, 1.0));
    rne_next_height(RNE_SIZE_PIXELS(16.0f, 1.0));
    rne_next_bg(sp_color_rgba_f(0.3f, 0.3f, 0.3f, 0.3f));
    RNE_Widget* widget = rne_widget(sp_str_pushf(arena, "%.*s-container", id.len, id.data),
            RNE_WIDGET_FLAG_NONE);

    SP_Vec2 widget_pos = widget->computed_absolute_position;
    SP_Vec2 widget_size = widget->computed_size;
    Rect nob = {0};
    nob.size = sp_v2s(widget_size.y);

    f32 bar_width = widget_size.x - nob.size.x;

    // Bar
    Rect bar = {0};
    bar.size = sp_v2(bar_width, 2.0f);
    bar.pos = widget_pos;
    bar.pos.x += nob.size.x / 2.0f;
    bar.pos.y += widget_size.y / 2.0f;
    bar.pos.y -= bar.size.y / 2.0f;
    bar.color = sp_color_rgba_f(0.3f, 0.3f, 0.3f, 1.0f);

    // Nob
    nob.pos = widget_pos;
    nob.pos.x += bar_width * ((*value - min) / (max - min));

    // Fill
    Rect fill = bar;
    fill.size.x = nob.pos.x - bar.pos.x + nob.size.x / 2.0f;
    fill.color = sp_color_rgba_f(0.75f, 0.75f, 0.75f, 1.0f);

    rne_next_width(RNE_SIZE_PIXELS(nob.size.x, 1.0f));
    rne_next_height(RNE_SIZE_PIXELS(nob.size.y, 1.0f));
    rne_next_fixed_x(nob.pos.x);
    rne_next_fixed_y(nob.pos.y);
    rne_next_bg(sp_color_rgba_f(1.0f, 1.0f, 1.0f, 1.0f));
    RNE_Widget* nob_widget = rne_widget(sp_str_pushf(arena, "%.*s-nob", id.len, id.data),
            RNE_WIDGET_FLAG_DRAW_BACKGROUND |
            RNE_WIDGET_FLAG_FLOATING |
            RNE_WIDGET_FLAG_INTERACTIVE);

    RNE_Signal signal = rne_signal(nob_widget);
    if (signal.focused) {
        *value += (mouse.pos_delta.x / bar_width) * (max - min);
    }
    *value = sp_clamp(*value, min, max);

    SliderData* data = sp_arena_push_no_zero(arena, sizeof(SliderData));
    *data = (SliderData) {
        .bar = bar,
        .fill = fill,
    };
    rne_widget_equip_render_func(widget, slider_render, data);

    return widget;
}

i32 main(void) {
    sp_init(SP_CONFIG_DEFAULT);
    glfwInit();
    SP_Arena* arena = sp_arena_create();
    sp_arena_tag(arena, sp_str_lit("main"));

    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, false);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Nuh Uh", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Load GL functions
    gladLoadGL(glfwGetProcAddress);

    // Set up GL state
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create renderer
    Renderer renderer = renderer_create(arena);
    NewRenderer nr = new_renderer_create();

    // Font* font = font_create(arena, sp_str_lit("assets/Roboto/Roboto-Regular.ttf"));
    // Font* font = font_create(arena, sp_str_lit("assets/Spline_Sans/static/SplineSans-Regular.ttf"));
    // Font* font = font_create(arena, sp_str_lit("assets/Tiny5/Tiny5-Regular.ttf"));
    Font* font = font_create(arena, sp_str_lit("assets/Roboto_Mono/static/RobotoMono-Regular.ttf"));

    rne_init((RNE_FontInterface) {
            .measure = measure,
            .query = query,
        }, (RNE_StyleStack) {
            .size = {
                [RNE_AXIS_HORIZONTAL] = {
                    .kind = RNE_SIZE_KIND_CHILDREN,
                    .value = 0.0f,
                    .strictness = 1.0f,
                },
                [RNE_AXIS_VERTICAL] = {
                    .kind = RNE_SIZE_KIND_CHILDREN,
                    .value = 0.0f,
                    .strictness = 1.0f,
                },
            },
            .fg = SP_COLOR_WHITE,
            .bg = SP_COLOR_WHITE,
            .font = font,
            .font_size = 24,
            .flow = RNE_AXIS_VERTICAL,
            .text_align = RNE_TEXT_ALIGN_LEFT,
        });

    u32 fps = 0;
    u32 last_fps = 0;
    f32 fps_timer = 0.0f;
    f32 last = sp_os_get_time();

    SP_Vec2 window_pos = sp_v2s(128.0f);
    b8 show_window = false;
    SP_Vec2 view_offset = sp_v2s(0.0f);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        f32 curr = sp_os_get_time();
        f32 dt = curr - last;
        last = curr;

        fps++;
        fps_timer += dt;
        if (fps_timer >= 1.0f) {
            last_fps = fps;
            fps = 0;
            fps_timer = 0.0f;
        }

        SP_Ivec2 screen_size;
        glfwGetFramebufferSize(window, &screen_size.x, &screen_size.y);

        reset_mouse();
        glfwPollEvents();

        // Build RNE_
        rne_begin(screen_size, mouse);
        SP_Arena* frame_arena = rne_get_arena();

        // Top bar
        rne_next_bg(sp_color_rgba_f(0.1f, 0.1f, 0.1f, 1.0f));
        rne_next_width(RNE_SIZE_PARENT(1.0f, 1.0f));
        rne_next_height(RNE_SIZE_CHILDREN(1.0f));
        rne_next_flow(RNE_AXIS_HORIZONTAL);
        RNE_Widget* container = rne_widget(sp_str_lit("container"), RNE_WIDGET_FLAG_DRAW_BACKGROUND | RNE_WIDGET_FLAG_VIEW_SCROLL);
        rne_push_parent(container);
        {
            spacer(RNE_SIZE_PIXELS(16.0f, 1.0));
            column() {
                spacer(RNE_SIZE_PIXELS(16.0f, 1.0));

                rne_next_width(RNE_SIZE_PIXELS(rne_top_font_size() * 6.0f, 1.0f));
                rne_next_height(RNE_SIZE_PIXELS(rne_top_font_size() * 2.0f, 1.0f));
                rne_next_bg(sp_color_rgba_f(0.75f, 0.2f, 0.2f, 1.0));
                rne_next_text_align(RNE_TEXT_ALIGN_CENTER);
                RNE_Widget* button = rne_widget(sp_str_lit("Show Window"), RNE_WIDGET_FLAG_DRAW_BACKGROUND | RNE_WIDGET_FLAG_DRAW_TEXT | RNE_WIDGET_FLAG_INTERACTIVE);
                RNE_Signal signal = rne_signal(button);
                if (signal.hovered) {
                    button->bg = sp_color_rgba_f(0.5f, 0.2f, 0.2f, 1.0);
                }
                if (signal.pressed) {
                    button->bg = sp_color_rgba_f(0.2f, 0.2f, 0.5f, 1.0);
                }
                if (signal.clicked) {
                    show_window = true;
                }

                spacer(RNE_SIZE_PIXELS(8.0f, 1.0));

                row() {
                    text(sp_str_lit("Window toggle: "));
                    checkbox(sp_str_pushf(frame_arena, "checkbock%p", &show_window), &show_window);
                }

                spacer(RNE_SIZE_PIXELS(8.0f, 1.0));

                row() {
                    static f32 value = 0.0f;
                    text(sp_str_pushf(frame_arena, "Slider: ", value));
                    slider(sp_str_lit("slidervalue"), &value, 5.0f, 10.0f);
                }

                spacer(RNE_SIZE_PIXELS(16.0f, 1.0));
            }

            spacer(RNE_SIZE_PARENT(1.0f, 0.0f));

            rne_push_width(RNE_SIZE_TEXT(1.0f));
            rne_push_height(RNE_SIZE_TEXT(1.0f));

            spacer(RNE_SIZE_PARENT(1.0f, 0.0f));

            rne_next_width(RNE_SIZE_CHILDREN(1.0));
            rne_next_height(RNE_SIZE_CHILDREN(1.0));
            column() {
                rne_widget(sp_str_pushf(rne_get_arena(), "FPS: %u", last_fps), RNE_WIDGET_FLAG_DRAW_TEXT);
                rne_widget(sp_str_pushf(rne_get_arena(), "Delta Time: %.4f", dt), RNE_WIDGET_FLAG_DRAW_TEXT);
            }
            spacer(RNE_SIZE_PIXELS(16.0f, 1.0f));

            rne_pop_width();
            rne_pop_height();
        }
        rne_pop_parent();

        // Draggable window
        if (show_window) {
            rne_next_bg(sp_color_rgba_f(0.2f, 0.2f, 0.3f, 1.0f));
            rne_next_width(RNE_SIZE_PIXELS(128.0f, 1.0f));
            rne_next_height(RNE_SIZE_PIXELS(128.0f, 1.0f));
            rne_next_fixed_x(window_pos.x);
            rne_next_fixed_y(window_pos.y);
            rne_next_text_align(RNE_TEXT_ALIGN_CENTER);
            RNE_Widget* draggable = rne_widget(sp_str_lit("Drag me!##window"),
                    RNE_WIDGET_FLAG_DRAW_BACKGROUND |
                    RNE_WIDGET_FLAG_FLOATING |
                    RNE_WIDGET_FLAG_DRAW_TEXT);
            rne_push_parent(draggable);
            {
                rne_next_bg(sp_color_rgba_f(0.0f, 0.0f, 0.0f, 0.5f));
                rne_next_width(RNE_SIZE_PARENT(1.0f, 1.0f));
                rne_next_height(RNE_SIZE_PIXELS(32.0f, 1.0f));
                // rne_next_height(RNE_SIZE_CHILDREN(1.0f));
                rne_next_flow(RNE_AXIS_HORIZONTAL);
                RNE_Widget* drag_bar = rne_widget(sp_str_lit("##dragbar"), RNE_WIDGET_FLAG_DRAW_BACKGROUND | RNE_WIDGET_FLAG_INTERACTIVE);
                RNE_Signal signal = rne_signal(drag_bar);
                if (signal.focused) {
                    window_pos = sp_v2_add(window_pos, signal.drag);
                }
                rne_push_parent(drag_bar);
                {
                    rne_next_height(RNE_SIZE_PARENT(1.0f, 1.0));
                    spacer(RNE_SIZE_PARENT(1.0f, 0.0f));

                    rne_next_width(RNE_SIZE_TEXT(1.0f));
                    rne_next_height(RNE_SIZE_PARENT(1.0f, 1.0f));
                    RNE_Widget* close_button = rne_widget(sp_str_lit("X##close_button"), RNE_WIDGET_FLAG_DRAW_TEXT | RNE_WIDGET_FLAG_INTERACTIVE);
                    if (rne_signal(close_button).clicked) {
                        show_window = false;
                    }

                    spacer(RNE_SIZE_PIXELS(8.0f, 1.0));
                }
                rne_pop_parent();

                rne_next_width(RNE_SIZE_PARENT(1.0f, 0.0f));
                rne_next_height(RNE_SIZE_PARENT(1.0f, 0.0f));
                RNE_Widget* content = rne_widget(sp_str_lit("##content"), RNE_WIDGET_FLAG_INTERACTIVE |
                        RNE_WIDGET_FLAG_OVERFLOW_Y |
                        RNE_WIDGET_FLAG_CLIP |
                        RNE_WIDGET_FLAG_VIEW_SCROLL);
                rne_signal(content);

                rne_push_parent(content);
                {
                    for (u32 i = 0; i < 32; i++) {
                        rne_next_bg(sp_color_rgba_f(1.0f / 32 * i, 0.3f, 0.3f, 1.0f));
                        rne_next_width(RNE_SIZE_PARENT(1.0f, 1.0f));
                        rne_next_height(RNE_SIZE_TEXT(1.0f));
                        rne_widget(sp_str_pushf(frame_arena, "%d", i), RNE_WIDGET_FLAG_DRAW_BACKGROUND | RNE_WIDGET_FLAG_DRAW_TEXT);
                    }
                }
                rne_pop_parent();
            }
            rne_pop_parent();
        }

        rne_end();

        // Render
        RNE_DrawCmdBuffer buffer = rne_draw(frame_arena);

        // RNE_DrawCmdBuffer buffer = rne_draw_buffer_begin(frame_arena);
        // rne_draw_scissor(&buffer, (RNE_DrawScissor) {
        //         .pos = sp_v2s(0.0f),
        //         .size = sp_v2s(256.0f),
        //     });

        // rne_draw_circle_filled(&buffer, (RNE_DrawCircle) {
        //         .pos = sp_v2s(256.0f),
        //         .radius = 32.0f,
        //         .color = SP_COLOR_GREEN,
        //         .segments = 32,
        //     });

        glViewport(0, 0, screen_size.x, screen_size.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glScissor(0, 0, screen_size.x, screen_size.y);

        RNE_BatchCmd batch;
        RNE_TessellationState state = {0};
        glBindVertexArray(nr.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, nr.ibo);
        glUseProgram(nr.shader);
        new_renderer_update_projection(&nr, screen_size);

        u32 batch_count = 0;
        u32 total_draw_count = 0;
        while ((batch = rne_tessellate(&buffer, (RNE_TessellationConfig) {
                .arena = rne_get_arena(),
                .vertex_buffer = nr.vertex_buffer,
                .vertex_capacity = sp_arrlen(nr.vertex_buffer),
                .index_buffer = nr.index_buffer,
                .index_capacity = sp_arrlen(nr.index_buffer),
            }, &state)).render_cmds != NULL) {
            new_renderer_update_buffers(&nr, batch.vertex_count, batch.index_count);
            u32 draw_count = 0;
            for (RNE_RenderCmd* cmd = batch.render_cmds; cmd != NULL; cmd = cmd->next) {
                glScissor((i32) cmd->scissor.pos.x,
                        (i32) (screen_size.y - cmd->scissor.size.y - cmd->scissor.pos.y),
                        cmd->scissor.size.x,
                        cmd->scissor.size.y);
                glDrawElements(GL_TRIANGLES, cmd->index_count, GL_UNSIGNED_SHORT, (const void*) (u64) cmd->start_offset_bytes);
                draw_count++;
            }
            total_draw_count += draw_count;
            batch_count++;
            sp_debug("draw_count = %d", draw_count);
        }
        sp_debug("batch_count = %d", batch_count);
        sp_debug("total_draw_count = %d", total_draw_count);

        glfwSwapBuffers(window);
    }

    // sp_dump_arena_metrics();

    // Shutdown
    font_destroy(font);
    new_renderer_destroy(&nr);
    renderer_destroy(&renderer);
    glfwTerminate();
    sp_terminate();
    return 0;
}
