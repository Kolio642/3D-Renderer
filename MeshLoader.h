#pragma once

#include "Types.h"

#include <vector>
#include <string>

struct LoadedMesh {
    std::vector <app::Vertex>     vertices;
    std::vector <uint32_t>        indices;
    std::vector <app::SubMesh>    subMeshes;
    std::vector <SDL_GPUTexture*> textures;
};

class MeshLoader 
{
public:
    static LoadedMesh LoadOBJ(SDL_GPUDevice* gpu,
                              const std::string& objPath,
                              const std::string& mtlDir);
private:
    static SDL_GPUTexture* LoadTexture(SDL_GPUDevice* gpu,
                                       const std::string& path);
};
 