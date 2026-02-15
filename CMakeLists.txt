cmake_minimum_required(VERSION 3.16)
project(utau_core_minimal VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(utau_core_render
    src/main.cpp
)

if (MSVC)
  target_compile_options(utau_core_render PRIVATE /W4)
else()
  target_compile_options(utau_core_render PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_test(
  NAME cli_help
  COMMAND utau_core_render --help
)

add_test(
  NAME render_minimal
  COMMAND utau_core_render --ust ${CMAKE_SOURCE_DIR}/examples/minimal.ust --out ${CMAKE_BINARY_DIR}/minimal.wav
)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(
  NAME validate_minimal_wav
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/verify_wav.py ${CMAKE_BINARY_DIR}/minimal.wav 44100 60000
)
set_tests_properties(validate_minimal_wav PROPERTIES DEPENDS render_minimal)

add_test(
  NAME render_rest_note
  COMMAND utau_core_render --ust ${CMAKE_SOURCE_DIR}/tests/rest_note.ust --out ${CMAKE_BINARY_DIR}/rest_note.wav --sr 48000
)
add_test(
  NAME validate_rest_note_wav
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/verify_wav.py ${CMAKE_BINARY_DIR}/rest_note.wav 48000 59000
)
set_tests_properties(validate_rest_note_wav PROPERTIES DEPENDS render_rest_note)
