#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <string>

class RenderApp {
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
        void HandleEvents(bool& running);
        void Cleanup();

        // Helpers
        std::vector <char> LoadShaderFile(const std::string& path);
        void UpdateDepthTexture(Uint32 width, Uint32 height);

        // Core SDL State
        SDL_Window* window = nullptr;
        SDL_GPUDevice* gpu = nullptr;

        // Graphics State
        SDL_GPUBuffer* vertexBuffer = nullptr;
        SDL_GPUShader* vertexShader = nullptr;
        SDL_GPUShader* fragmentShader = nullptr;
        SDL_GPUGraphicsPipeline* pipeline = nullptr;

        // Depth Texture State
        SDL_GPUTexture* depthTexture = nullptr;
        Uint32 currentDepthWidth = 0;
        Uint32 currentDepthHeight = 0;

};