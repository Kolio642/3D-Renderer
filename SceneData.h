#pragma once

#include <SDL3/SDL.h>

#include <iostream>

#include "Types.h"

class SceneData 
{
public:
    void UploadMesh(SDL_GPUDevice* gpu, const app::LoadedMesh& mesh);
    ~SceneData();

    // Acessories for the render loop
    SDL_GPUBuffer* GetVertexBuffer() const;
    SDL_GPUBuffer* GetIndexBuffer() const;
    const std::vector <app::SubMesh>& GetSubMeshes() const;
    SDL_GPUTexture* GetTexture(int materialId) const;
    SDL_GPUTexture* GetFallBackTexture() const;

private:
    SDL_GPUDevice* gpu = nullptr; // Non-owning to differentiate for cleanup
    SDL_GPUBuffer* vertexBuffer = nullptr;
    SDL_GPUBuffer* indexBuffer = nullptr;
    std::vector <app::SubMesh> subMeshes;
    std::vector <SDL_GPUTexture *> textures;
    SDL_GPUTexture* fallBackTexture = nullptr;
};