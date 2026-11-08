cmake_minimum_required(VERSION 3.22.1)
project(voxelcraft_rails LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(voxelcraft_rails SHARED
    src/voxelcraft_rails.cpp
    src/voxelcraft_rails.h
)

target_include_directories(voxelcraft_rails PUBLIC src)
target_compile_options(voxelcraft_rails PRIVATE -Wall -Wextra -O2)

# Ensure 16 KB page alignment required by Android 15+ devices
target_link_options(voxelcraft_rails PRIVATE
    -Wl,-z,max-page-size=16384
)
