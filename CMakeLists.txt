cmake_minimum_required(VERSION 3.24)
project(3DRenderer LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(glm REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    RenderApp.cpp
    GPUContext.cpp
    MeshLoader.cpp
    SceneData.cpp
    ShaderManager.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 glm::glm)

# Copy shaders next to the executable so "shaders/triangle.msl" resolves at runtime
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
)