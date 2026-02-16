cmake_minimum_required(VERSION 3.16)
project(MiniUtauCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mini_utau
    src/main.cpp
    src/ust_parser.cpp
    src/oto_parser.cpp
    src/synth_engine.cpp
)

target_include_directories(mini_utau PRIVATE include)
