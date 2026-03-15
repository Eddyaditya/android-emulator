cmake_minimum_required(VERSION 3.10)

# Project name
project(EDU_Emulator)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add source files
set(SOURCES 
    main.cpp
    emulator.cpp
    emulator.h
)

# Create an executable
add_executable(edu_emulator ${SOURCES})

# Link libraries if needed
# target_link_libraries(edu_emulator some_library)
