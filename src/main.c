#include "spire.h"
#include "renderer.h"
#include "font.h"
#include "ui.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

static void column_start(void) {
    ui_next_width(UI_SIZE_CHILDREN(1.0f));
    ui_next_height(UI_SIZE_CHILDREN(1.0f));
    ui_next_flow(UI_AXIS_VERTICAL);
    UIWidget* column = ui_widget(sp_str_lit(""), UI_WIDGET_FLAG_NONE);
    ui_push_parent(column);
}

static void column_end(void) {
    ui_pop_parent();
}

static void row_start(void) {
    ui_next_width(UI_SIZE_CHILDREN(1.0f));
    ui_next_height(UI_SIZE_CHILDREN(1.0f));
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
            break;
        case UI_AXIS_VERTICAL:
            ui_next_height(size);
            break;
        case UI_AXIS_COUNT:
            break;
    }
    return ui_widget(sp_str_lit(""), UI_WIDGET_FLAG_NONE);
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
        });

    u32 fps = 0;
    u32 last_fps = 0;
    f32 fps_timer = 0.0f;
    f32 last = sp_os_get_time();

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

        // Build UI
        ui_begin(screen_size);

        ui_next_bg(sp_v4(0.1f, 0.1f, 0.1f, 1.0f));
        ui_next_width(UI_SIZE_PIXELS(screen_size.x, 1.0f));
        ui_next_height(UI_SIZE_CHILDREN(1.0f));
        ui_next_flow(UI_AXIS_HORIZONTAL);
        UIWidget* container = ui_widget(sp_str_lit("container"), UI_WIDGET_FLAG_DRAW_BACKGROUND);
        ui_push_parent(container);
        {
            ui_push_width(UI_SIZE_TEXT(1.0f));
            ui_push_height(UI_SIZE_TEXT(1.0f));

            spacer(UI_SIZE_PIXELS(16.0f, 1.0f));
            column() {
                ui_widget(sp_str_pushf(ui_get_arena(), "FPS: %u", last_fps), UI_WIDGET_FLAG_DRAW_TEXT);
                ui_widget(sp_str_pushf(ui_get_arena(), "Delta Time: %.4f", dt), UI_WIDGET_FLAG_DRAW_TEXT);
            }

            spacer(UI_SIZE_PARENT(1.0, 0.0f));
            ui_widget(sp_str_lit("More things"), UI_WIDGET_FLAG_DRAW_TEXT);

            spacer(UI_SIZE_PARENT(1.0, 0.0f));
            ui_widget(sp_str_lit("Right"), UI_WIDGET_FLAG_DRAW_TEXT);
            spacer(UI_SIZE_PIXELS(16.0f, 1.0f));

            ui_pop_width();
            ui_pop_height();
        }
        ui_pop_parent();

        // ui_next_bg(sp_v4(0.1f, 0.1f, 0.1f, 1.0f));
        // ui_next_width(UI_SIZE_CHILDREN(1.0f));
        // ui_next_height(UI_SIZE_CHILDREN(1.0f));
        // UIWidget* container = ui_widget(sp_str_lit("container"), UI_WIDGET_FLAG_DRAW_BACKGROUND);
        // ui_push_parent(container);
        // {
        //     column() {
        //         spacer(UI_SIZE_PIXELS(16.0f, 1.0f));
        //         row() {
        //             spacer(UI_SIZE_PIXELS(16.0f, 1.0f));
        //
        //             ui_push_width(UI_SIZE_TEXT(1.0f));
        //             ui_push_height(UI_SIZE_TEXT(1.0f));
        //
        //             column() {
        //                 ui_widget(sp_str_pushf(ui_get_arena(), "FPS: %u", last_fps), UI_WIDGET_FLAG_DRAW_TEXT);
        //                 ui_widget(sp_str_lit("Abc"), UI_WIDGET_FLAG_DRAW_TEXT);
        //                 ui_widget(sp_str_lit("Wow"), UI_WIDGET_FLAG_DRAW_TEXT);
        //             }
        //
        //             spacer(UI_SIZE_PIXELS(16.0f, 1.0f));
        //
        //             column() {
        //                 ui_widget(sp_str_lit("Another column!"), UI_WIDGET_FLAG_DRAW_TEXT);
        //                 ui_widget(sp_str_lit("This is amazing."), UI_WIDGET_FLAG_DRAW_TEXT);
        //                 ui_widget(sp_str_lit("I love this so much :3"), UI_WIDGET_FLAG_DRAW_TEXT);
        //                 ui_widget(sp_str_lit("I'm so proud of myself :D"), UI_WIDGET_FLAG_DRAW_TEXT);
        //             }
        //
        //             ui_pop_width();
        //             ui_pop_height();
        //
        //             spacer(UI_SIZE_PIXELS(16.0f, 1.0f));
        //         }
        //         spacer(UI_SIZE_PIXELS(16.0f, 1.0f));
        //     }
        // }
        // ui_pop_parent();

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
        glfwPollEvents();
    }

    sp_dump_arena_metrics();

    // Shutdown
    font_destroy(font);
    renderer_destroy(&renderer);
    glfwTerminate();
    sp_terminate();
    return 0;
}
