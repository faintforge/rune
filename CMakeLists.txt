cmake_minimum_required(VERSION 3.16)
project(rune VERSION 0.1 LANGUAGES C)

option(RUNE_BUILD_EXAMPLES "Build examples" false)
option(BUILD_SHARED_LIBS "Build shared instead of static libraries" false)
option(RUNE_INCLUDE_FONT "Include font module in build" true)
option(RUNE_INCLUDE_TESSELLATION "Include tessellation module in build" true)

add_library(rune STATIC src/rune.c)

target_include_directories(rune
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(rune PUBLIC c_std_99)
target_compile_options(rune
    PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
        $<$<C_COMPILER_ID:MSVC>:/W4>
)

if (NOT TARGET spire)
    add_subdirectory(spire)
endif ()

target_link_libraries(rune spire)

if (RUNE_INCLUDE_FONT)
    target_include_directories(rune PRIVATE third_party/stb)
    target_sources(rune PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/rune_font.c)
endif ()

if (RUNE_INCLUDE_TESSELLATION)
    target_sources(rune PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/rune_tessellation.c)
endif ()

include(GNUInstallDirs)

install(TARGETS rune
    EXPORT runeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT runeTargets
    FILE runeTargets.cmake
    NAMESPACE rune::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rune
)

if (RUNE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()
