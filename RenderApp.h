#pragma once

#include <SDL3/SDL.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vector>
#include <string>

// Internal Constants & Structs
namespace app
{
    const int WIDTH = 800;
    const int HEIGHT = 600;
    const float CAMERA_DISTANCE = 4.0f;
    const float CAMERA_HEIGHT = 1.5f;
    const float WORLD_SPIN_SPEED = 0.8f;

    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
    };

    struct LightUniforms
    {
        glm::vec4 direction;
        glm::vec4 color;
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
    // Lifestyle phases
    bool InitSDL();
    bool CreateMesh();
    bool CreatePipeline();
    void RenderFrame();
    void HandleEvents(bool &running);
    void Cleanup();

    // Helpers
    std::vector<char> LoadShaderFile(const std::string &path);
    void UpdateDepthTexture(Uint32 width, Uint32 height);

    // Global light
    app::LightUniforms mainLight;

    // Core SDL State
    SDL_Window *window = nullptr;
    SDL_GPUDevice *gpu = nullptr;

    // Graphics State
    SDL_GPUBuffer *vertexBuffer = nullptr;
    SDL_GPUBuffer *indexBuffer = nullptr;
    SDL_GPUShader *vertexShader = nullptr;
    SDL_GPUShader *fragmentShader = nullptr;
    SDL_GPUGraphicsPipeline *pipeline = nullptr;

    // Depth Texture State
    SDL_GPUTexture *depthTexture = nullptr;
    Uint32 currentDepthWidth = 0;
    Uint32 currentDepthHeight = 0;

    Uint32 vertexCount = 0;
    Uint32 indexCount = 0;
};