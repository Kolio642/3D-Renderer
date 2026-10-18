#include "Types.h"

#include "SceneData.h"

#include <vector>

void SceneData::UploadMesh(SDL_GPUDevice *gpu, const app::LoadedMesh &mesh)
{
    this->gpu = gpu;
    // 1. Create the FallBackTexture

    SDL_GPUTextureCreateInfo fallbackInfo{};
    fallbackInfo.type = SDL_GPU_TEXTURETYPE_2D;
    fallbackInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    fallbackInfo.width = 1;
    fallbackInfo.height = 1;
    fallbackInfo.layer_count_or_depth = 1;
    fallbackInfo.num_levels = 1;
    fallbackInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    fallBackTexture = SDL_CreateGPUTexture(gpu, &fallbackInfo);

    SDL_GPUTransferBufferCreateInfo fallbackTransferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, 4};
    SDL_GPUTransferBuffer *fallbackTransfer = SDL_CreateGPUTransferBuffer(gpu, &fallbackTransferInfo);

    // 4 bytes: R=255, G=255, B=255, A=255 (Solid White)
    Uint8 whitePixel[4] = {255, 255, 255, 255};
    void *map = SDL_MapGPUTransferBuffer(gpu, fallbackTransfer, false);
    std::memcpy(map, whitePixel, 4);
    SDL_UnmapGPUTransferBuffer(gpu, fallbackTransfer);

    SDL_GPUCommandBuffer *fallbackCmd = SDL_AcquireGPUCommandBuffer(gpu);
    SDL_GPUCopyPass *fallbackCopy = SDL_BeginGPUCopyPass(fallbackCmd);

    SDL_GPUTextureTransferInfo fallbackSource{};
    fallbackSource.transfer_buffer = fallbackTransfer;
    fallbackSource.offset = 0;

    SDL_GPUTextureRegion fallbackDest{};
    fallbackDest.texture = fallBackTexture;
    fallbackDest.w = 1;
    fallbackDest.h = 1;
    fallbackDest.d = 1;

    SDL_UploadToGPUTexture(fallbackCopy, &fallbackSource, &fallbackDest, false);
    SDL_EndGPUCopyPass(fallbackCopy);
    SDL_SubmitGPUCommandBuffer(fallbackCmd);
    SDL_ReleaseGPUTransferBuffer(gpu, fallbackTransfer);
    
    // 2. Keep ownership of submeshes and textures for rendering
    subMeshes = std::move(mesh.subMeshes);
    textures = std::move(mesh.textures);

    // 3. Vertex Buffer setup
    uint32_t vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    size_t vertexBufferSize = vertexCount * sizeof(app::Vertex);

    SDL_GPUBufferCreateInfo vertexBufferInfo{SDL_GPU_BUFFERUSAGE_VERTEX, (Uint32)vertexBufferSize};
    vertexBuffer = SDL_CreateGPUBuffer(gpu, &vertexBufferInfo);

    SDL_GPUTransferBufferCreateInfo vertexTransferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, (Uint32)vertexBufferSize};
    SDL_GPUTransferBuffer *vertexTransferBuffer = SDL_CreateGPUTransferBuffer(gpu, &vertexTransferInfo);

    void *vertexMapPtr = SDL_MapGPUTransferBuffer(gpu, vertexTransferBuffer, false);
    std::memcpy(vertexMapPtr, mesh.vertices.data(), vertexBufferSize);
    SDL_UnmapGPUTransferBuffer(gpu, vertexTransferBuffer);

    // 4. Index Buffer setup
    uint32_t indexCount = static_cast<uint32_t>(mesh.indices.size());
    size_t indexBufferSize = indexCount * sizeof(Uint32);

    SDL_GPUBufferCreateInfo indexBufferInfo{SDL_GPU_BUFFERUSAGE_INDEX, (Uint32)indexBufferSize};
    indexBuffer = SDL_CreateGPUBuffer(gpu, &indexBufferInfo);

    SDL_GPUTransferBufferCreateInfo indicesTransferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, (Uint32)indexBufferSize};
    SDL_GPUTransferBuffer *indicesTransferBuffer = SDL_CreateGPUTransferBuffer(gpu, &indicesTransferInfo);

    void *indicesMapPtr = SDL_MapGPUTransferBuffer(gpu, indicesTransferBuffer, false);
    std::memcpy(indicesMapPtr, mesh.indices.data(), indexBufferSize);
    SDL_UnmapGPUTransferBuffer(gpu, indicesTransferBuffer);

    // 5. Upload both Buffers to GPU
    SDL_GPUCommandBuffer *uploadCmdBuf = SDL_AcquireGPUCommandBuffer(gpu);
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

    SDL_GPUTransferBufferLocation vertexSource = {vertexTransferBuffer, 0};
    SDL_GPUBufferRegion vertexDestination = {vertexBuffer, 0, (Uint32)vertexBufferSize};
    SDL_UploadToGPUBuffer(copyPass, &vertexSource, &vertexDestination, false);

    SDL_GPUTransferBufferLocation indicesSource = {indicesTransferBuffer, 0};
    SDL_GPUBufferRegion indicesDestination = {indexBuffer, 0, (Uint32)indexBufferSize};
    SDL_UploadToGPUBuffer(copyPass, &indicesSource, &indicesDestination, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmdBuf);

    SDL_ReleaseGPUTransferBuffer(gpu, vertexTransferBuffer);
    SDL_ReleaseGPUTransferBuffer(gpu, indicesTransferBuffer);
}

SceneData::~SceneData()
{
    if (!gpu)
        return;

    if (gpu)
        SDL_WaitForGPUIdle(gpu);

    for (SDL_GPUTexture *tex : textures)
        if (tex != nullptr)
            SDL_ReleaseGPUTexture(gpu, tex);
    textures.clear();

    if (fallBackTexture)
        SDL_ReleaseGPUTexture(gpu, fallBackTexture);

    if (vertexBuffer)
        SDL_ReleaseGPUBuffer(gpu, vertexBuffer);
    if (indexBuffer)
        SDL_ReleaseGPUBuffer(gpu, indexBuffer);
}

SDL_GPUBuffer *SceneData::GetVertexBuffer() const
{
    return vertexBuffer;
}

SDL_GPUBuffer *SceneData::GetIndexBuffer() const
{
    return indexBuffer;
}

const std::vector<app::SubMesh> &SceneData::GetSubMeshes() const
{
    return subMeshes;
}

SDL_GPUTexture *SceneData::GetTexture(int materialId) const
{
    if (materialId >= 0 && materialId < textures.size() && textures[materialId] != nullptr)
        return textures[materialId];
    
    return fallBackTexture;
}

SDL_GPUTexture *SceneData::GetFallBackTexture() const
{
    return fallBackTexture;
}