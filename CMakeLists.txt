cmake_minimum_required(VERSION 3.20)

project(KeySequence LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(KeySequence main.cpp)

target_compile_definitions(KeySequence PRIVATE UNICODE _UNICODE)
