cmake_minimum_required(VERSION 3.16)
project(ui LANGUAGES C)

add_subdirectory("spire/")
add_subdirectory("third_party/glad/")
add_subdirectory("third_party/stb/")

file(GLOB_RECURSE SOURCE CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_executable(${PROJECT_NAME} ${SOURCE})

target_include_directories(${PROJECT_NAME}
    PRIVATE "src/"
    PRIVATE "include/"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED false
    C_EXTENSIONS false
    EXPORT_COMPILE_COMMANDS true
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_link_libraries(${PROJECT_NAME} spire stb glad glfw m)

if (MSVC)
    add_compile_options("/W4")
else ()
    add_compile_options("-Wall -Wextra -Wpedantic")
endif ()
