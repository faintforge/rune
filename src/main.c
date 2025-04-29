#include "spire.h"
#include "renderer.h"
#include "font.h"
#include "ui.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

static UIMouse mouse = {0};
static void reset_mouse(void) {
    mouse.pos_delta = sp_v2s(0.0f);
    mouse.scroll = 0.0f;
    for (UIMouseButton btn = 0; btn < UI_MOUSE_BUTTON_COUNT; btn++) {
        mouse.buttons[btn].clicked = false;
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    switch (button) {
        case GLFW_MOUSE_BUTTON_1:
            mouse.buttons[UI_MOUSE_BUTTON_LEFT].pressed = action;
            mouse.buttons[UI_MOUSE_BUTTON_LEFT].clicked = action;
            break;
        case GLFW_MOUSE_BUTTON_2:
            mouse.buttons[UI_MOUSE_BUTTON_RIGHT].pressed = action;
            mouse.buttons[UI_MOUSE_BUTTON_RIGHT].clicked = action;
            break;
        case GLFW_MOUSE_BUTTON_3:
            mouse.buttons[UI_MOUSE_BUTTON_MIDDLE].pressed = action;
            mouse.buttons[UI_MOUSE_BUTTON_MIDDLE].clicked = action;
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
    // ui_next_width(UI_SIZE_CHILDREN(1.0f));
    // ui_next_height(UI_SIZE_CHILDREN(1.0f));
    ui_next_flow(UI_AXIS_VERTICAL);
    UIWidget* column = ui_widget(sp_str_lit(""), UI_WIDGET_FLAG_NONE);
    ui_push_parent(column);
}

static void column_end(void) {
    ui_pop_parent();
}

static void row_start(void) {
    // ui_next_width(UI_SIZE_CHILDREN(1.0f));
    // ui_next_height(UI_SIZE_CHILDREN(1.0f));
    ui_next_flow(UI_AXIS_HORIZONTAL);
    UIWidget* row = ui_widget(sp_str_lit(""), UI_WIDGET_FLAG_NONE);
    ui_push_parent(row);
}

static void row_end(void) {
    ui_pop_parent();
}

#define DEFER(BEGIN, END) for (b8 _i_ = (BEGIN, false); !_i_; _i_ = (END, true))
#define column() DEFER(column_start(), column_end())
#define row() DEFER(row_start(), row_end())

static UIWidget* spacer(UISize size) {
    UIAxis flow = ui_top_parent()->flow;
    switch (flow) {
        case UI_AXIS_HORIZONTAL:
            ui_next_width(size);
            ui_next_height(UI_SIZE_PIXELS(0.0f, 0.0f));
            break;
        case UI_AXIS_VERTICAL:
            ui_next_width(UI_SIZE_PIXELS(0.0f, 0.0f));
            ui_next_height(size);
            break;
        case UI_AXIS_COUNT:
            break;
    }
    return ui_widget(sp_str_lit(""), UI_WIDGET_FLAG_NONE);
}

static UIWidget* text(SP_Str text) {
    ui_next_width(UI_SIZE_TEXT(1.0));
    ui_next_height(UI_SIZE_TEXT(1.0));
    return ui_widget(text, UI_WIDGET_FLAG_DRAW_TEXT);
}

static void checkbox_render(UIWidget* widget, Renderer* renderer, void* userdata) {
    b8 value = *(b8*) userdata;
    if (!value) {
        return;
    }

    const f32 fill_amount = 0.6f;

    SP_Vec2 size = widget->computed_size;
    size = sp_v2_muls(size, fill_amount);

    SP_Vec2 pos = widget->computed_absolute_position;
    pos = sp_v2_add(pos, sp_v2_divs(sp_v2_sub(widget->computed_size, size), 2.0f));

    renderer_draw(renderer, (RenderBox) {
            .pos = pos,
            .size = size,
            .color = sp_v4(0.75f, 0.75f, 0.75f, 1.0f),
        });
}

static UIWidget* checkbox(SP_Str id, b8* value) {
    ui_next_width(UI_SIZE_PIXELS(32.0f, 1.0f));
    ui_next_height(UI_SIZE_PIXELS(32.0f, 1.0f));
    ui_next_bg(sp_v4(0.2f, 0.2f, 0.2f, 1.0f));
    UIWidget* checkbox = ui_widget(id, UI_WIDGET_FLAG_DRAW_BACKGROUND | UI_WIDGET_FLAG_INTERACTIVE);
    ui_widget_equip_render_func(checkbox, checkbox_render, value);

    UISignal signal = ui_signal(checkbox);
    if (signal.clicked) {
        *value = !*value;
    }

    return checkbox;
}

typedef struct Rect Rect;
struct Rect {
    SP_Vec2 pos;
    SP_Vec2 size;
    SP_Vec4 color;
};

typedef union SliderData SliderData;
union SliderData {
    struct {
        Rect bar;
        Rect fill;
    };
    Rect r[2];
};

static void slider_render(UIWidget* widget, Renderer* renderer, void* userdata) {
    SliderData* data = userdata;
    for (u8 i = 0; i < sp_arrlen(data->r); i++) {
        renderer_draw(renderer, (RenderBox) {
                .pos = data->r[i].pos,
                .size = data->r[i].size,
                .color = data->r[i].color,
            });
    }
}

static UIWidget* slider(SP_Str id, f32* value, f32 min, f32 max) {
    SP_Arena* arena = ui_get_arena();

    ui_next_width(UI_SIZE_PIXELS(128.0f, 1.0));
    ui_next_height(UI_SIZE_PIXELS(16.0f, 1.0));
    ui_next_bg(sp_v4s(0.3f));
    UIWidget* widget = ui_widget(sp_str_pushf(arena, "%.*s-container", id.len, id.data),
            UI_WIDGET_FLAG_NONE);

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
    bar.color = sp_v4(0.3f, 0.3f, 0.3f, 1.0f);

    // Nob
    nob.pos = widget_pos;
    nob.pos.x += bar_width * ((*value - min) / (max - min));

    // Fill
    Rect fill = bar;
    fill.size.x = nob.pos.x - bar.pos.x + nob.size.x / 2.0f;
    fill.color = sp_v4(0.75f, 0.75f, 0.75f, 1.0f);

    ui_next_width(UI_SIZE_PIXELS(nob.size.x, 1.0f));
    ui_next_height(UI_SIZE_PIXELS(nob.size.y, 1.0f));
    ui_next_fixed_x(nob.pos.x);
    ui_next_fixed_y(nob.pos.y);
    ui_next_bg(sp_v4(1.0f, 1.0f, 1.0f, 1.0f));
    UIWidget* nob_widget = ui_widget(sp_str_pushf(arena, "%.*s-nob", id.len, id.data),
            UI_WIDGET_FLAG_DRAW_BACKGROUND |
            UI_WIDGET_FLAG_FLOATING |
            UI_WIDGET_FLAG_INTERACTIVE);

    UISignal signal = ui_signal(nob_widget);
    if (signal.focused) {
        *value += (mouse.pos_delta.x / bar_width) * (max - min);
    }
    *value = sp_clamp(*value, min, max);

    SliderData* data = sp_arena_push_no_zero(arena, sizeof(SliderData));
    *data = (SliderData) {
        .bar = bar,
        .fill = fill,
    };
    ui_widget_equip_render_func(widget, slider_render, data);

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
    // Font* font = font_create(arena, sp_str_lit("assets/Roboto/Roboto-Regular.ttf"));
    // Font* font = font_create(arena, sp_str_lit("assets/Spline_Sans/static/SplineSans-Regular.ttf"));
    // Font* font = font_create(arena, sp_str_lit("assets/Tiny5/Tiny5-Regular.ttf"));
    Font* font = font_create(arena, sp_str_lit("assets/Roboto_Mono/static/RobotoMono-Regular.ttf"));

    ui_init((UIStyleStack) {
            .size = {
                [UI_AXIS_HORIZONTAL] = {
                    .kind = UI_SIZE_KIND_CHILDREN,
                    .value = 0.0f,
                    .strictness = 1.0f,
                },
                [UI_AXIS_VERTICAL] = {
                    .kind = UI_SIZE_KIND_CHILDREN,
                    .value = 0.0f,
                    .strictness = 1.0f,
                },
            },
            .fg = sp_v4s(1.0f),
            .bg = sp_v4s(1.0f),
            .font = font,
            .font_size = 24,
            .flow = UI_AXIS_VERTICAL,
            .text_align = UI_TEXT_ALIGN_LEFT,
        });

    u32 fps = 0;
    u32 last_fps = 0;
    f32 fps_timer = 0.0f;
    f32 last = sp_os_get_time();

    SP_Vec2 window_pos = sp_v2s(128.0f);
    b8 show_window = false;

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

        // Build UI
        ui_begin(screen_size, mouse);

        // Top bar
        ui_next_bg(sp_v4(0.1f, 0.1f, 0.1f, 1.0f));
        ui_next_width(UI_SIZE_PARENT(1.0f, 1.0f));
        ui_next_height(UI_SIZE_CHILDREN(1.0f));
        ui_next_flow(UI_AXIS_HORIZONTAL);
        UIWidget* container = ui_widget(sp_str_lit("container"), UI_WIDGET_FLAG_DRAW_BACKGROUND);
        ui_push_parent(container);
        {
            spacer(UI_SIZE_PIXELS(16.0f, 1.0));
            column() {
                spacer(UI_SIZE_PIXELS(16.0f, 1.0));

                ui_next_width(UI_SIZE_PIXELS(ui_top_font_size() * 6.0f, 1.0f));
                ui_next_height(UI_SIZE_PIXELS(ui_top_font_size() * 2.0f, 1.0f));
                ui_next_bg(sp_v4(0.75f, 0.2f, 0.2f, 1.0));
                ui_next_text_align(UI_TEXT_ALIGN_CENTER);
                UIWidget* button = ui_widget(sp_str_lit("Show Window"), UI_WIDGET_FLAG_DRAW_BACKGROUND | UI_WIDGET_FLAG_DRAW_TEXT);
                UISignal signal = ui_signal(button);
                if (signal.hovered) {
                    button->bg = sp_v4(0.5f, 0.2f, 0.2f, 1.0);
                }
                if (signal.pressed) {
                    button->bg = sp_v4(0.2f, 0.2f, 0.5f, 1.0);
                }
                if (signal.clicked) {
                    show_window = true;
                }

                spacer(UI_SIZE_PIXELS(8.0f, 1.0));

                row() {
                    text(sp_str_lit("Window toggle: "));
                    checkbox(sp_str_pushf(arena, "checkbock%p", &show_window), &show_window);
                }

                spacer(UI_SIZE_PIXELS(8.0f, 1.0));

                row() {
                    static f32 value = 0.0f;
                    text(sp_str_pushf(arena, "Slider: ", value));
                    slider(sp_str_lit("slidervalue"), &value, 5.0f, 10.0f);
                }

                spacer(UI_SIZE_PIXELS(16.0f, 1.0));
            }

            spacer(UI_SIZE_PARENT(1.0f, 0.0f));

            ui_push_width(UI_SIZE_TEXT(1.0f));
            ui_push_height(UI_SIZE_TEXT(1.0f));

            spacer(UI_SIZE_PARENT(1.0f, 0.0f));

            ui_next_width(UI_SIZE_CHILDREN(1.0));
            ui_next_height(UI_SIZE_CHILDREN(1.0));
            column() {
                ui_widget(sp_str_pushf(ui_get_arena(), "FPS: %u", last_fps), UI_WIDGET_FLAG_DRAW_TEXT);
                ui_widget(sp_str_pushf(ui_get_arena(), "Delta Time: %.4f", dt), UI_WIDGET_FLAG_DRAW_TEXT);
            }
            spacer(UI_SIZE_PIXELS(16.0f, 1.0f));

            ui_pop_width();
            ui_pop_height();
        }
        ui_pop_parent();

        // Draggable window
        if (show_window) {
            ui_next_bg(sp_v4(0.2f, 0.2f, 0.3f, 1.0f));
            ui_next_width(UI_SIZE_PIXELS(128.0f, 1.0f));
            ui_next_height(UI_SIZE_PIXELS(128.0f, 1.0f));
            ui_next_fixed_x(window_pos.x);
            ui_next_fixed_y(window_pos.y);
            ui_next_text_align(UI_TEXT_ALIGN_CENTER);
            UIWidget* draggable = ui_widget(sp_str_lit("Drag me!##window"),
                    UI_WIDGET_FLAG_DRAW_BACKGROUND |
                    UI_WIDGET_FLAG_FLOATING |
                    UI_WIDGET_FLAG_DRAW_TEXT);
            ui_push_parent(draggable);
            {
                ui_next_bg(sp_v4(0.0f, 0.0f, 0.0f, 0.5f));
                ui_next_width(UI_SIZE_PARENT(1.0f, 1.0f));
                ui_next_height(UI_SIZE_PIXELS(32.0f, 1.0f));
                // ui_next_height(UI_SIZE_CHILDREN(1.0f));
                ui_next_flow(UI_AXIS_HORIZONTAL);
                UIWidget* drag_bar = ui_widget(sp_str_lit("##dragbar"), UI_WIDGET_FLAG_DRAW_BACKGROUND);
                UISignal signal = ui_signal(drag_bar);
                if (signal.focused) {
                    window_pos = sp_v2_add(window_pos, signal.drag);
                }
                ui_push_parent(drag_bar);
                {
                    ui_next_height(UI_SIZE_PARENT(1.0f, 1.0));
                    spacer(UI_SIZE_PARENT(1.0f, 0.0f));

                    ui_next_width(UI_SIZE_TEXT(1.0f));
                    ui_next_height(UI_SIZE_PARENT(1.0f, 1.0f));
                    UIWidget* close_button = ui_widget(sp_str_lit("X##close_button"), UI_WIDGET_FLAG_DRAW_TEXT);
                    if (ui_signal(close_button).clicked) {
                        show_window = false;
                    }

                    spacer(UI_SIZE_PIXELS(8.0f, 1.0));
                }
                ui_pop_parent();
            }
            ui_pop_parent();
        }

        ui_end();

        // Render
        glViewport(0.0f, 0.0f, screen_size.x, screen_size.y);
        glScissor(0, 0, screen_size.x, screen_size.y);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        renderer_begin(&renderer, screen_size);
        ui_draw(&renderer);
        renderer_end(&renderer);

        glfwSwapBuffers(window);
    }

    // sp_dump_arena_metrics();

    // Shutdown
    font_destroy(font);
    renderer_destroy(&renderer);
    glfwTerminate();
    sp_terminate();
    return 0;
}
