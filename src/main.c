#include "font.h"
#include "spire.h"
#include "renderer.h"

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

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        SP_Ivec2 screen_size;
        glfwGetFramebufferSize(window, &screen_size.x, &screen_size.y);

        // Render
        glViewport(0.0f, 0.0f, screen_size.x, screen_size.y);
        glScissor(0, 0, screen_size.x, screen_size.y);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        renderer_begin(&renderer, screen_size);

        renderer_draw_text(&renderer, sp_v2(0.0f, 0.0f), sp_str_lit("The quick brown fox jumps over the lazy dog"), font, 32.0f, sp_v4s(1.0f));

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
