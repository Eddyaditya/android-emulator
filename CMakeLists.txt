cmake_minimum_required(VERSION 3.10)

# Project name
project(EDU_Emulator)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Make headers in src/ available via unqualified #include "..."
include_directories(${CMAKE_SOURCE_DIR}/src)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Source files
set(SOURCES
    main.cpp
    src/emulator.cpp
    src/qemu_config.cpp
    src/qemu_manager.cpp
    src/ui_manager.cpp
    src/ui_window.cpp
    src/frame_buffer.cpp
    src/input_handler.cpp
    src/gapps_config.cpp
    src/gapps_manager.cpp
    src/play_store.cpp
    src/adb_config.cpp
    src/adb_manager.cpp
    src/root_manager.cpp
)

# Create the executable
add_executable(edu_emulator ${SOURCES})

# Link against SDL2 and platform threading library
target_link_libraries(edu_emulator ${SDL2_LIBRARIES} Threads::Threads)
