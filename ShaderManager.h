#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

class ShaderManager
{
public:
    // Loads, configures, and compiles an MSL shader from disk for SDL_GPU
    static SDL_GPUShader* LoadShader(SDL_GPUDevice* gpu,
                                     const std::string& path,
                                     const char* entryPoint,
                                     SDL_GPUShaderStage stage,
                                     Uint32 numUniformBuffers,
                                     Uint32 numSamplers = 0,
                                     Uint32 numStorageBuffers = 0,
                                     Uint32 numStorageTextures = 0);

private:
    // Helper to read raw bytes from disk
    static std::vector<char> ReadFile(const std::string& path);
};