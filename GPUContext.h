#pragma once

#include <SDL3/SDL.h>

class GPUContext
{
public:
    bool Initialize(const char* title, float w, float h);
    ~GPUContext();

    SDL_Window* GetWindow() const;
    SDL_GPUDevice* GetDevice() const;

private:
    void Cleanup();
    SDL_Window* window = nullptr;
    SDL_GPUDevice* gpu = nullptr;
};