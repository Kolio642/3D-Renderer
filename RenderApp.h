#pragma once

#include "Camera.h"
#include "GPUContext.h"
#include "MeshLoader.h"
#include "Types.h"

#include <SDL3/SDL.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vector>
#include <string>

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
    bool CreateMesh();
    bool CreatePipeline();
    void RenderFrame();
    void HandleEvents(bool &running);
    void Cleanup();

    // Helpers
    std::vector<char> LoadShaderFile(const std::string &path);
    void UpdateDepthTexture(Uint32 width, Uint32 height);

    // Global light
    app::FragmentUniforms mainLight;

    // Core SDL State
    GPUContext ctx;

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