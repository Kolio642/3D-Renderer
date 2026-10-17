#include "GPUContext.h"

#include <iostream>

SDL_Window* GPUContext::GetWindow() const{
    return window;
}

SDL_GPUDevice* GPUContext::GetDevice() const {
    return gpu;
}

bool GPUContext::Initialize(const char* title, float w, float h) {

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << '\n';
        return false;
    }

    window = SDL_CreateWindow("3D Renderer", w, h, SDL_WINDOW_RESIZABLE);
    if (!window)
        return false;

    gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, true, nullptr);
    if (!gpu)
        return false;

    if (!SDL_ClaimWindowForGPUDevice(gpu, window))
        return false;

    std::cout << "GPU Device initialized succesfully using: " << SDL_GetGPUDeviceDriver(gpu) << '\n';
    
    return true;
} 

void GPUContext::Cleanup() {
    if(gpu && window) {
        SDL_ReleaseWindowFromGPUDevice(gpu, window);
    }
    if(window) {
        SDL_DestroyWindow(window);
    }
    if(gpu) {
        SDL_DestroyGPUDevice(gpu);
    }
}

GPUContext::~GPUContext() {
    Cleanup();

    SDL_Quit();
}