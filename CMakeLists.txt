cmake_minimum_required(VERSION 3.21)
project(IPK\ |\ HTTP\ server C)
set(CMAKE_SOURCE_DIR src/)
set(PROJECT_BINARY_DIR ../build/bin/)
set(CMAKE_BINARY_DIR ../build/bin/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../build/bin/)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -pedantic -Wall -Wextra -fsanitize=address")

add_executable(http_server src/hinfosvc.c src/http-processing.c src/http-processing.h src/system-info.c src/system-info.h)
