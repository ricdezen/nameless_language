cmake_minimum_required(VERSION 3.19)
project(nameless_language C)

set(CMAKE_C_STANDARD 99)

set(MAIN_SRC src/chunk.c src/main.c src/memory.c src/debug.c)

add_executable(nameless_language ${MAIN_SRC})