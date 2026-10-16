#pragma once

#include "Camera.h"

#include <SDL3/SDL.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vector>
#include <string>

// Internal Constants & Structs
namespace app
{
    const int WIDTH = 800;
    const int HEIGHT = 600;
    const float CAMERA_DISTANCE = 4.0f;
    const float CAMERA_HEIGHT = 1.5f;
    const float WORLD_SPIN_SPEED = 0.6f;
    const float CAMERA_SPEED = 2.5f;
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

}

class RenderApp
{
public:
    RenderApp() = default;
    ~RenderApp();

    bool Initialize();
    void Run();

private:
    // Camera
    Camera camera {glm::vec3(0.0f, app::CAMERA_HEIGHT, app::CAMERA_DISTANCE)};
    bool mouseCaptured = false;
    Uint64 lastFrameTime = 0;

    // Lifestyle phases
    bool InitSDL();
    bool CreateMesh();
    bool CreatePipeline();
    void RenderFrame();
    void HandleEvents(bool &running);
    void UpdateCamera(float deltaTime);
    void Cleanup();

    // Helpers
    SDL_GPUTexture* LoadTexture(const std::string &path);
    std::vector<char> LoadShaderFile(const std::string &path);
    void UpdateDepthTexture(Uint32 width, Uint32 height);

    // Global light
    app::FragmentUniforms mainLight;

    // Core SDL State
    SDL_Window *window = nullptr;
    SDL_GPUDevice *gpu = nullptr;

    // Graphics State
    SDL_GPUBuffer *vertexBuffer = nullptr;
    SDL_GPUBuffer *indexBuffer = nullptr;
    SDL_GPUShader *vertexShader = nullptr;
    SDL_GPUShader *fragmentShader = nullptr;
    SDL_GPUGraphicsPipeline *pipeline = nullptr;

    std::vector <app::SubMesh> subMeshes;            // Stores the draw ranges for specific Texture
    std::vector <SDL_GPUTexture*> materialTextures;  // Stores loaded Texture material
    SDL_GPUTexture* whiteFallbackTexture = nullptr;  // Used when a material has no texture
    SDL_GPUSampler* sampler = nullptr;               // Tells the GPU how to read the pixels   

    // Depth Texture State
    SDL_GPUTexture *depthTexture = nullptr;
    Uint32 currentDepthWidth = 0;
    Uint32 currentDepthHeight = 0;

    Uint32 vertexCount = 0;
    Uint32 indexCount = 0;
};