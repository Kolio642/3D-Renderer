#pragma once

#include "Types.h"

#include <vector>
#include <string>

class MeshLoader 
{
public:
    static app::LoadedMesh LoadOBJ(SDL_GPUDevice* gpu,
                              const std::string& objPath,
                              const std::string& mtlDir = "");
private:
    static SDL_GPUTexture* LoadTexture(SDL_GPUDevice* gpu,
                                       const std::string& path);
};
 