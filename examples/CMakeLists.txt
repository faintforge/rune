cmake_minimum_required(VERSION 3.16)
project(rune_examples LANGUAGES C)

add_subdirectory("third_party/glad/")

find_library(GLFW_LIB glfw REQUIRED)
add_executable(hello_world renderer.c hello_world.c)
target_link_libraries(hello_world rune glad ${GLFW_LIB})
