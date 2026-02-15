cmake_minimum_required(VERSION 3.16)
project(PyUtauContinuedCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(utau_core
    src/engine/encoding.cpp
    src/engine/oto.cpp
    src/engine/ust.cpp
    src/engine/voicebank.cpp
    src/engine/synthesis.cpp
    src/engine/wav_writer.cpp
)

target_include_directories(utau_core PUBLIC include)

add_executable(utau_cli src/main.cpp)
target_link_libraries(utau_cli PRIVATE utau_core)
