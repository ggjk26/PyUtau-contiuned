# 设置最低 CMake 版本要求
cmake_minimum_required(VERSION 3.21)

set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64/lib/cmake/Qt6")

# 设置项目名称和支持的语言
set(CMAKE_C_COMPILER cl)
set(CMAKE_CXX_COMPILER cl)
project(PyUtauContinued LANGUAGES CXX)

# 使用 C++20 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows MSVC 运行时库配置
if(MSVC)
    cmake_policy(SET CMP0091 NEW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# 查找 Qt6 库（确保你已安装 Qt6）
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# 自动配置 Qt 项目（启用 MOC、UIC、RCC 等）
qt_standard_project_setup()

# 设置源文件列表
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

# 设置头文件列表
set(HEADERS
    src/core/Project.h
    src/core/UstParser.h
    src/core/Voicebank.h
    src/core/Resampler.h
    src/audio/AudioEngine.h
    src/gui/MainWindow.h
    src/gui/PianoRollWidget.h
)

# 创建可执行目标
qt_add_executable(pyutau_continued ${SOURCES} ${HEADERS})

# 设定包含路径（通常不需要显式设置，除非有自定义路径）
target_include_directories(pyutau_continued PRIVATE src)

# 链接 Qt6 库到目标
target_link_libraries(pyutau_continued PRIVATE Qt6::Widgets)

# ------------------------
# 安装步骤（可选）
# ------------------------

# 如果你希望安装应用或库，可以通过以下方式设置
# install(TARGETS pyutau_continued RUNTIME DESTINATION bin)

# ------------------------
# 设置 Windows 平台的额外配置
# ------------------------

if(WIN32)
    # 设置 Windows 平台生成的目标为没有控制台窗口（隐藏黑框）
    set_target_properties(pyutau_continued PROPERTIES WIN32_EXECUTABLE ON)

    # 如果有额外的库路径或需要配置某些 Windows 特定的步骤，可以在这里添加
    # 例如部署 Qt 运行时库等操作
endif()

# ------------------------
# 编译设置（可选，特定优化）
# ------------------------

# 开启多线程并行构建（可选）
set(CMAKE_BUILD_PARALLEL_LEVEL 4)

# 使用 NINJA 作为生成器（可选，增加构建速度，特别适合 CI）
# set(CMAKE_GENERATOR "Ninja")
