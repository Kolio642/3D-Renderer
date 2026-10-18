#include "ShaderManager.h"

#include <iostream>
#include <fstream>

std::vector<char> ShaderManager::ReadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open shader file: " << path << '\n';
        return {};
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize + 1);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    buffer[fileSize] = '\0';

    return buffer;
}

SDL_GPUShader* ShaderManager::LoadShader(SDL_GPUDevice* gpu,
                                         const std::string& path,
                                         const char* entryPoint,
                                         SDL_GPUShaderStage stage,
                                         Uint32 numUniformBuffers,
                                         Uint32 numSamplers,
                                         Uint32 numStorageBuffers,
                                         Uint32 numStorageTextures)
{
    std::vector<char> shaderCode = ReadFile(path);
    if (shaderCode.empty())
    {
        return nullptr;
    }

    // In MSL, the code size passed to SDL_GPU should exclude the null terminator
    size_t codeSize = shaderCode.size() - 1;

    SDL_GPUShaderCreateInfo info{};
    info.code = reinterpret_cast<const Uint8*>(shaderCode.data());
    info.code_size = codeSize;
    info.entrypoint = entryPoint;
    info.format = SDL_GPU_SHADERFORMAT_MSL;
    info.stage = stage;
    info.num_uniform_buffers = numUniformBuffers;
    info.num_samplers = numSamplers;
    info.num_storage_buffers = numStorageBuffers;
    info.num_storage_textures = numStorageTextures;

    SDL_GPUShader* shader = SDL_CreateGPUShader(gpu, &info);
    if (!shader)
    {
        std::cerr << "Failed to create GPU shader [" << entryPoint << "] from " << path
                  << " Error: " << SDL_GetError() << '\n';
        return nullptr;
    }

    return shader;
}