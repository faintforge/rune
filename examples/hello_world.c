#include "rune/rune.h"
#include "rune/rune_font.h"
#include "rune/rune_tessellation.h"
#include "spire.h"
#include "renderer.h"
#include "font_data.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

static RNE_Mouse mouse = {0};
static void reset_mouse(void) {
    mouse.scroll = 0.0f;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void) window;
    (void) mods;
    switch (button) {
        case GLFW_MOUSE_BUTTON_1:
            mouse.buttons[RNE_MOUSE_BUTTON_LEFT] = action;
            break;
        case GLFW_MOUSE_BUTTON_2:
            mouse.buttons[RNE_MOUSE_BUTTON_RIGHT] = action;
            break;
        case GLFW_MOUSE_BUTTON_3:
            mouse.buttons[RNE_MOUSE_BUTTON_MIDDLE] = action;
            break;
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    (void) window;
    mouse.pos = sp_v2(xpos, ypos);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void) window;
    (void) xoffset;
    mouse.scroll += yoffset;
}

RNE_UserData atlas_create(SP_Ivec2 size) {
    u32 texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Swizzle components
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

    SP_Scratch scratch = sp_scratch_begin(NULL, 0);
    u8* zero_buffer = sp_arena_push(scratch.arena, size.x * size.y);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_R8,
            size.x,
            size.y,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            zero_buffer);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    sp_scratch_end(scratch);

    return (RNE_UserData) {
        .id = texture
    };
}

void atlas_destroy(RNE_UserData userdata) {
    u32 texture = userdata.id;
    glDeleteTextures(1, &texture);
}

void atlas_resize(RNE_UserData userdata, SP_Ivec2 size, const u8* pixels) {
    u32 texture = userdata.id;
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
            0,
            GL_R8,
            size.x,
            size.y,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void atlas_update(RNE_UserData userdata, SP_Ivec2 pos, SP_Ivec2 size, u32 stride, const u8* pixels) {
    (void) stride;
    u32 texture = userdata.id;
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D,
            0,
            pos.x,
            pos.y,
            size.x,
            size.y,
            GL_RED,
            GL_UNSIGNED_BYTE,
            pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void draw_border_func(RNE_DrawCmdBuffer* buffer, RNE_Widget* widget, void* userdata) {
    rne_draw_rect_stroke(buffer, (RNE_DrawRect) {
            .pos = widget->computed_absolute_position,
            .size = widget->computed_outer_size,
            .color = widget->fg,
            .corner_radius = widget->corner_radius,
            .corner_segments = 8,
        }, 2.0f);
}

i32 main(void) {
    sp_init(SP_CONFIG_DEFAULT);
    glfwInit();
    SP_Arena* arena = sp_arena_create();
    sp_arena_tag(arena, sp_str_lit("main"));

    // Gruvbox Colors
    // https://github.com/morhetz/gruvbox
    const SP_Color GB_BG = sp_color_rgb_hex(0x282828);
    const SP_Color GB_BG_H = sp_color_rgb_hex(0x1d2021);
    const SP_Color GB_BG1 = sp_color_rgb_hex(0x3c3836);
    const SP_Color GB_BG2 = sp_color_rgb_hex(0x504945);
    const SP_Color GB_FG = sp_color_rgb_hex(0xebdbb2);
    const SP_Color GB_RED = sp_color_rgb_hex(0xfb4934);
    const SP_Color GB_GREEN = sp_color_rgb_hex(0xb8bb26);
    const SP_Color GB_YELLOW = sp_color_rgb_hex(0xfabd2f);
    const SP_Color GB_BLUE = sp_color_rgb_hex(0x83a598);

    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, false);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Nuh Uh", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Load GL functions
    gladLoadGL(glfwGetProcAddress);

    // Set up GL state
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RNE_FontCallbacks font_callbacks = {
        .create = atlas_create,
        .destroy = atlas_destroy,
        .resize = atlas_resize,
        .update = atlas_update,
    };

    RNE_Handle font = rne_font_create(arena, ttf_data, font_callbacks);

    rne_init((RNE_StyleStack) {
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
            .font_size = 32.0f,
            .flow = RNE_AXIS_VERTICAL,
            .text_align = RNE_TEXT_ALIGN_LEFT,
        }, rne_text_measure);

    u32 fps = 0;
    u32 last_fps = 0;
    f32 fps_timer = 0.0f;
    f32 last = sp_os_get_time();

    Renderer nr = renderer_create();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        SP_Arena* frame_arena = rne_get_frame_arena();

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

        reset_mouse();
        glfwPollEvents();

        SP_Ivec2 screen_size;
        glfwGetFramebufferSize(window, &screen_size.x, &screen_size.y);
        SP_Vec2 screen_size_f = sp_iv2_to_v2(screen_size);

        rne_begin(screen_size, mouse);

        rne_next_width(RNE_SIZE_TEXT(1.0f));
        rne_next_height(RNE_SIZE_TEXT(1.0f));
        rne_next_padding(sp_v4s(16.0f));
        rne_next_fg(GB_FG);
        rne_next_offset(rne_offset(sp_v2(0.0f, 0.0f), sp_v2(0.0f, 1.0f)));
        rne_widget(sp_str_pushf(sp_arena_allocator(frame_arena), "FPS: %d", last_fps), RNE_WIDGET_FLAG_DRAW_TEXT |
                RNE_WIDGET_FLAG_FIXED);

        rne_next_offset(rne_offset(sp_v2s(16.0f), sp_v2s(0.0f)));
        rne_next_width(RNE_SIZE_CHILDREN(1.0f));
        rne_next_height(RNE_SIZE_CHILDREN(1.0f));
        rne_next_bg(GB_BG_H);
        rne_next_padding(sp_v4s(16.0f));
        rne_next_corner_radius(sp_v4s(16.0f));
        RNE_Widget* column = rne_widget(RNE_NULL_ID, RNE_WIDGET_FLAG_DRAW_BACKGROUND |
                RNE_WIDGET_FLAG_FIXED);
        rne_push_parent(column);
        {
            rne_next_width(RNE_SIZE_TEXT(1.0f));
            rne_next_height(RNE_SIZE_TEXT(1.0f));
            rne_next_fg(sp_color_hsv(sp_os_get_time() * 180.0f, 0.75f, 1.0f));
            rne_next_font_size(48.0f);
            rne_widget(sp_str_lit("Rune"), RNE_WIDGET_FLAG_DRAW_TEXT);

            rne_next_bg(GB_BG);
            rne_next_fg(GB_GREEN);
            rne_next_width(RNE_SIZE_TEXT(1.0f));
            rne_next_height(RNE_SIZE_TEXT(1.0f));
            rne_next_padding(sp_v4s(8.0f));
            rne_next_text_align(RNE_TEXT_ALIGN_CENTER);
            rne_next_corner_radius(sp_v4(4.0f, 16.0f, 16.0f, 4.0f));
            rne_next_offset(rne_offset(sp_v2(8.0f, 16.0f), sp_v2s(0.0f)));
            RNE_Widget* interactive = rne_widget(sp_str_lit("Interactive!"), RNE_WIDGET_FLAG_DRAW_TEXT |
                    RNE_WIDGET_FLAG_DRAW_BACKGROUND);
            rne_widget_equip_render_func(interactive, draw_border_func, NULL);

            RNE_Signal signal = rne_signal(interactive);
            if (signal.hovered) { interactive->fg = GB_RED; }
            if (signal.just_focused) { sp_info("Focused"); }
            if (signal.just_lost_focus) { sp_info("Lost focus"); }

            if (signal.focused) { interactive->fg = GB_BLUE; }
            if (signal.active) { interactive->fg = GB_YELLOW; }
            if (signal.pressed) { interactive->bg = GB_BG_H; }
            if (signal.just_pressed) { sp_info("Pressed"); }
            if (signal.just_released) { sp_info("Released"); }

            rne_next_width(RNE_SIZE_TEXT(1.0f));
            rne_next_height(RNE_SIZE_TEXT(1.0f));
            rne_widget(sp_str_lit("duplicate"), RNE_WIDGET_FLAG_DRAW_TEXT);

            rne_next_width(RNE_SIZE_CHILDREN(1.0f));
            rne_next_height(RNE_SIZE_CHILDREN(1.0f));
            RNE_Widget* other_container = rne_widget(sp_str_lit("some_id"), RNE_WIDGET_FLAG_DRAW_BACKGROUND);
            rne_push_parent(other_container);
            {
                rne_next_width(RNE_SIZE_TEXT(1.0f));
                rne_next_height(RNE_SIZE_TEXT(1.0f));
                rne_next_fg(SP_COLOR_BLACK);
                rne_widget(sp_str_lit("duplicate"), RNE_WIDGET_FLAG_DRAW_TEXT);
            }
            rne_pop_parent();
        }
        rne_pop_parent();

        rne_end();

        // Render
        RNE_DrawCmdBuffer buffer = rne_draw(frame_arena);

        glViewport(0, 0, screen_size.x, screen_size.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        RNE_BatchCmd batch;
        RNE_TessellationState* state = NULL;
        glBindVertexArray(nr.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, nr.ibo);
        glUseProgram(nr.shader);
        renderer_update_projection(&nr, screen_size);
        RNE_Handle textures[8] = {0};
        while ((batch = rne_tessellate(&buffer, (RNE_TessellationConfig) {
                .arena = frame_arena,
                .font = RNE_FONT_INTERFACE,
                .vertex_buffer = nr.vertex_buffer,
                .vertex_capacity = sp_arrlen(nr.vertex_buffer),
                .index_buffer = nr.index_buffer,
                .index_capacity = sp_arrlen(nr.index_buffer),
                .texture_buffer = textures,
                .texture_capacity = sp_arrlen(textures),
                .null_texture = {
                    .id = nr.null_texture,
                },
            }, &state)).render_cmds != NULL) {
            // Set up OpenGL state
            renderer_update_buffers(&nr, batch.vertex_count, batch.index_count);
            for (u32 i = 0; i < batch.texture_count; i++) {
                glBindTextureUnit(i, textures[i].id);
            }

            // Render
            for (RNE_RenderCmd* cmd = batch.render_cmds; cmd != NULL; cmd = cmd->next) {
                glScissor((i32) cmd->scissor.pos.x,
                        (i32) (screen_size.y - cmd->scissor.size.y - cmd->scissor.pos.y),
                        cmd->scissor.size.x,
                        cmd->scissor.size.y);
                glDrawElements(GL_TRIANGLES, cmd->index_count, GL_UNSIGNED_SHORT, (const void*) (u64) cmd->start_offset_bytes);
            }
        }

        glfwSwapBuffers(window);
    }
}
