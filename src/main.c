#include "spire.h"
#include "renderer.h"
#include "font.h"
#include "ui.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

i32 main(void) {
    sp_init(SP_CONFIG_DEFAULT);
    glfwInit();
    SP_Arena* arena = sp_arena_create();

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
    Font* font = font_create(arena, sp_str_lit("assets/Roboto/Roboto-Regular.ttf"));
    // Font* font = font_create(arena, sp_str_lit("assets/Spline_Sans/static/SplineSans-Light.ttf"));
    // Font* font = font_create(arena, sp_str_lit("assets/Tiny5/Tiny5-Regular.ttf"));

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

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        SP_Ivec2 screen_size;
        glfwGetFramebufferSize(window, &screen_size.x, &screen_size.y);

        // Build UI
        ui_begin(screen_size);

        ui_next_bg(sp_v4(0.5f, 0.0f, 0.0f, 1.0f));
        ui_next_width((UISize) {
                .kind = UI_SIZE_KIND_CHILDREN,
                .strictness = 1.0f,
            });
        ui_next_height((UISize) {
                .kind = UI_SIZE_KIND_CHILDREN,
                .strictness = 1.0f,
            });
        UIWidget* container = ui_widget(sp_str_lit("container"),
                UI_WIDGET_FLAG_DRAW_BACKGROUND);
        ui_push_parent(container);

        ui_next_font_size(32);
        ui_next_width((UISize) {
                .kind = UI_SIZE_KIND_TEXT,
                .strictness = 1.0f,
            });
        ui_next_height((UISize) {
                .kind = UI_SIZE_KIND_TEXT,
                .strictness = 1.0f,
            });
        ui_widget(sp_str_lit("The quick brown fox jumps over the lazy dog!##this is just the id"),
                UI_WIDGET_FLAG_DRAW_TEXT);

        ui_next_flow(UI_AXIS_HORIZONTAL);
        ui_next_bg(sp_v4(0.0f, 0.5f, 0.0f, 1.0f));
        UIWidget* row = ui_widget(sp_str_lit("row"), UI_WIDGET_FLAG_DRAW_BACKGROUND);
        ui_push_parent(row);

        ui_next_width((UISize) { .kind = UI_SIZE_KIND_PIXELS, .value = 64.0f, .strictness = 1.0f });
        ui_next_height((UISize) { .kind = UI_SIZE_KIND_TEXT, .strictness = 1.0f });
        ui_widget(sp_str_lit("padding"), UI_WIDGET_FLAG_NONE);

        ui_next_width((UISize) { .kind = UI_SIZE_KIND_TEXT, .strictness = 1.0f });
        ui_next_height((UISize) { .kind = UI_SIZE_KIND_TEXT, .strictness = 1.0f });
        ui_widget(sp_str_lit("Another one"),
                UI_WIDGET_FLAG_DRAW_TEXT);

        ui_next_fixed_x(128.0f);
        ui_next_fixed_y(128.0f);
        ui_next_width((UISize) { .kind = UI_SIZE_KIND_PIXELS, .value = 128.0f, .strictness = 1.0f });
        ui_next_height((UISize) { .kind = UI_SIZE_KIND_PIXELS, .value = 128.0f, .strictness = 1.0f });
        ui_next_bg(sp_v4(0.0f, 0.0f, 0.5f, 1.0f));
        ui_widget(sp_str_lit("Floating Box"),
                UI_WIDGET_FLAG_DRAW_TEXT |
                UI_WIDGET_FLAG_DRAW_BACKGROUND |
                UI_WIDGET_FLAG_DRAW_FLOATING);


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

    // Shutdown
    font_destroy(font);
    renderer_destroy(&renderer);
    glfwTerminate();
    sp_terminate();
    return 0;
}
