cmake_minimum_required(VERSION 3.31)

add_library(stb STATIC)
target_include_directories(stb PUBLIC ".")
target_sources(stb PRIVATE "stb.c")
