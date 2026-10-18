#pragma once

#include <SDL3/SDL.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

// Internal Constants & Structs
namespace app
{
    const int WIDTH = 1200;
    const int HEIGHT = 800;
    const float CAMERA_DISTANCE = 4.0f;
    const float CAMERA_HEIGHT = 1.5f;
    const float WORLD_SPIN_SPEED = 0.6f;
    const float CAMERA_SPEED = 1.5f;
    const float MOUSE_SENSITIVITY = 0.1f;

    struct SubMesh
    {
        Uint32 indexOffset;
        Uint32 indexCount;
        int materialId;
    };
    
    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 uv; // Uvs (Texture Coordinates)
    };

    // Data for the Vertex Shader
    struct VertexUniforms
    {
        glm::mat4 mvp;
        glm::mat4 model;
    };

    // Data for the Fragment Shader
    struct FragmentUniforms
    {
        glm::vec4 lightDirection;
        glm::vec4 lightColor;
    };
    struct LoadedMesh
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<SubMesh> subMeshes;
        std::vector<SDL_GPUTexture *> textures;
    };
}