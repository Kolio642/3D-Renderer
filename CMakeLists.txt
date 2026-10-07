cmake_minimum_required(VERSION 3.24)
project(3d-voxel-engine LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(glm REQUIRED)

add_executable(3d-voxel-engine
    main.cpp
    RenderApp.cpp
)

target_include_directories(3d-voxel-engine PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(3d-voxel-engine PRIVATE SDL3::SDL3 glm::glm)

# Copy shaders next to the executable so "shaders/triangle.msl" resolves at runtime
add_custom_command(TARGET 3d-voxel-engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:3d-voxel-engine>/shaders
)