#include "new_renderer.h"

#include <glad/gl.h>

NewRenderer new_renderer_create(void) {
    NewRenderer renderer = {0};

    // Vertex buffer
    u64 vert_size = sizeof(renderer.vertex_buffer);
    glGenBuffers(1, &renderer.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, renderer.vbo);
    glBufferData(GL_ARRAY_BUFFER, vert_size, renderer.vertex_buffer, GL_DYNAMIC_DRAW);

    // Index buffer
    u64 index_size = sizeof(renderer.index_buffer);
    glGenBuffers(1, &renderer.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size, renderer.index_buffer, GL_DYNAMIC_DRAW);

    // Vertex array
    glGenVertexArrays(1, &renderer.vao);
    glBindVertexArray(renderer.vao);

    // Vertex layout
    // Position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(RNE_Vertex), (const void*) sp_offset(RNE_Vertex, pos));
    glEnableVertexAttribArray(0);
    // UV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(RNE_Vertex), (const void*) sp_offset(RNE_Vertex, uv));
    glEnableVertexAttribArray(1);
    // Color
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(RNE_Vertex), (const void*) sp_offset(RNE_Vertex, color));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Shader
    const SP_Str vertex_source = sp_str_lit(
            "#version 330 core\n"
            "layout (location = 0) in vec2 aPos;\n"
            "layout (location = 1) in vec2 aUv;\n"
            "layout (location = 2) in vec4 aColor;\n"
            "out vec4 color;\n"
            "uniform mat4 projection;\n"
            "void main() {\n"
            "    color = aColor;\n"
            "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
            "}\n"
        );
    const SP_Str fragment_source = sp_str_lit(
            "#version 330 core\n"
            "layout (location = 0) out vec4 fragColor;\n"
            "in vec4 color;\n"
            "void main() {\n"
            "    fragColor = color;\n"
            "}\n"
        );

    i32 success = 0;
    char info_log[512] = {0};

    u32 v_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v_shader, 1, (const char* const*) &vertex_source.data, (const int*) &vertex_source.len);
    glCompileShader(v_shader);
    glGetShaderiv(v_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(v_shader, sizeof(info_log), NULL, info_log);
        sp_error("Vertex shader compilation error: %s", info_log);
    }

    u32 f_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f_shader, 1, (const char* const*) &fragment_source.data, (const int*) &fragment_source.len);
    glCompileShader(f_shader);
    glGetShaderiv(f_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(f_shader, sizeof(info_log), NULL, info_log);
        sp_error("Fragment shader compilation error: %s\n", info_log);
    }

    renderer.shader = glCreateProgram();
    glAttachShader(renderer.shader, v_shader);
    glAttachShader(renderer.shader, f_shader);
    glLinkProgram(renderer.shader);
    glGetProgramiv(renderer.shader, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(renderer.shader, 512, NULL, info_log);
        sp_error("Shader linking error: %s\n", info_log);
    }

    glDeleteShader(v_shader);
    glDeleteShader(f_shader);

    return renderer;
}

void new_renderer_destroy(NewRenderer* renderer) {
    glDeleteVertexArrays(1, &renderer->vao);
    glDeleteBuffers(1, &renderer->vbo);
    glDeleteBuffers(1, &renderer->ibo);
    glDeleteProgram(renderer->shader);
}

void new_renderer_update_buffers(NewRenderer* renderer, u32 vertex_count, u32 index_count) {
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(RNE_Vertex) * vertex_count, renderer->vertex_buffer);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->ibo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(u16) * index_count, renderer->index_buffer);
}

void new_renderer_update_projection(NewRenderer* renderer, SP_Ivec2 screen_size) {
    f32 zoom = screen_size.y / 2.0f;
    f32 aspect = (f32) screen_size.x / screen_size.y;
    SP_Mat4 projection = sp_m4_ortho_projection(-aspect * zoom, aspect * zoom, -zoom, zoom, 1.0f, -1.0f);
    projection.d.x = -1.0f;
    projection.d.y = 1.0f;
    u32 loc = glGetUniformLocation(renderer->shader, "projection");
    glUniformMatrix4fv(loc, 1, false, &projection.a.x);
}
