cmake_minimum_required(VERSION 3.10)

# Project name
project(EDU_Emulator)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Make headers in src/ available via unqualified #include "..."
include_directories(${CMAKE_SOURCE_DIR}/src)

# Source files
set(SOURCES
    main.cpp
    src/emulator.cpp
    src/qemu_config.cpp
    src/qemu_manager.cpp
    src/ui_manager.cpp
)

# Create the executable
add_executable(edu_emulator ${SOURCES})

# Link libraries if needed
# target_link_libraries(edu_emulator some_library)
