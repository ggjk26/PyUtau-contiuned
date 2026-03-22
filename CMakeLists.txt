cmake_minimum_required(VERSION 3.21)
project(PyUtauContinued LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/core/Project.cpp
    src/core/UstParser.cpp
    src/core/Voicebank.cpp
    src/core/Resampler.cpp
    src/audio/AudioEngine.cpp
    src/gui/MainWindow.cpp
    src/gui/PianoRollWidget.cpp
)

set(HEADERS
    src/core/Project.h
    src/core/UstParser.h
    src/core/Voicebank.h
    src/core/Resampler.h
    src/audio/AudioEngine.h
    src/gui/MainWindow.h
    src/gui/PianoRollWidget.h
)

qt_add_executable(pyutau_continued
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(pyutau_continued PRIVATE src)

target_link_libraries(pyutau_continued PRIVATE Qt6::Widgets)
