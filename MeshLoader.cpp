#include "MeshLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vector>
#include <iostream>
#include <string>

struct VertexKey
{
    tinyobj::index_t idx;
    int material_id;
};

// Custom Hasher for tinyobj::index_t as a map key
struct VertexKeyHasher
{
    size_t operator()(const VertexKey &key) const
    {
        return (((size_t)key.idx.normal_index) ^
                ((size_t)key.idx.texcoord_index << 1) ^
                ((size_t)key.idx.vertex_index << 2)) ^
                ((size_t)key.material_id << 3);
    }
};

struct VertexKeyEqual
{
    bool operator()(const VertexKey &a, const VertexKey &b) const
    {
        return a.idx.vertex_index == b.idx.vertex_index &&
               a.idx.normal_index == b.idx.normal_index &&
               a.idx.texcoord_index == b.idx.texcoord_index &&
               a.material_id == b.material_id;
    }
};

app::LoadedMesh MeshLoader::LoadOBJ(SDL_GPUDevice* gpu,
                              const std::string& objPath,
                              const std::string& mtlDir) 
{
    app::LoadedMesh result;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objPath.c_str(), mtlDir.c_str()))
    {
        std::cerr << "Failed to load OBJ " << warn << err << '\n';
    }

    result.textures.resize(materials.size(), nullptr);
    for (size_t i = 0; i < materials.size(); i++)
    {
        if (!materials[i].diffuse_texname.empty())
        {
            std::string texPath = mtlDir + materials[i].diffuse_texname;
            result.textures[i] = LoadTexture(gpu, texPath);
            if (result.textures[i] != nullptr)
            {
                std::cout << "Loaded texture: " << texPath << '\n';
            }
        }
    }

    std::vector<app::Vertex> vertices;

    // Map: material_id -> list of indices using the same Texture (material)
    std::unordered_map<int, std::vector<Uint32>> materialIndices;

    // Center and scale the model
    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::min());

    std::unordered_map<VertexKey, Uint32, VertexKeyHasher, VertexKeyEqual> uniqueVertices;

    for (const auto &shape : shapes)
    {
        size_t index_offset = 0;

        // Loop over faces (polygons)
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
        {
            // Number of vertices in this face
            size_t fv = size_t(shape.mesh.num_face_vertices[f]);

            // Get the material ID for this face
            int material_id = shape.mesh.material_ids[f];

            // Extract the diffuse color from the material (default to white)
            glm::vec3 matColor = glm::vec3(1.0f, 1.0f, 1.0f);
            if (material_id >= 0 && material_id < materials.size())
            {
                if(materials[material_id].diffuse_texname.empty())
                {
                    matColor = glm::vec3(
                    materials[material_id].diffuse[0],
                    materials[material_id].diffuse[1],
                    materials[material_id].diffuse[2]);
                }   
            }

            for (size_t v = 0; v < fv; v++)
            {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                VertexKey key{idx, material_id};

                // If we haven't seen this vertex add it to the create one
                if (uniqueVertices.count(key) == 0)
                {
                    app::Vertex vertex{};
                    vertex.position = {
                        attrib.vertices[3 * idx.vertex_index + 0],
                        attrib.vertices[3 * idx.vertex_index + 1],
                        attrib.vertices[3 * idx.vertex_index + 2]};

                    // Map normals to RGB colors
                    if (idx.normal_index >= 0)
                    {
                        vertex.normal = {
                            (attrib.normals[3 * idx.normal_index + 0]),
                            (attrib.normals[3 * idx.normal_index + 1]),
                            (attrib.normals[3 * idx.normal_index + 2])};
                    }

                    vertex.color = matColor;

                    if (idx.texcoord_index >= 0)
                    {
                        vertex.uv = {
                            attrib.texcoords[2 * idx.texcoord_index + 0],
                            1.0f - attrib.texcoords[2 * idx.texcoord_index + 1] // Flip the y to wotk with Metal/SDL_GPU
                        };
                    }

                    minBounds = glm::min(minBounds, vertex.position);
                    maxBounds = glm::max(maxBounds, vertex.position);

                    // Add new vertex to array
                    vertices.push_back(vertex);

                    // Record it's location in the hash map
                    uniqueVertices[key] = static_cast<Uint32>(vertices.size() - 1);
                }

                materialIndices[material_id].push_back(uniqueVertices[key]);
            }

            index_offset += fv;
        }
    }

    // Flatten the Indices-Map into a single indices map
    std::vector<Uint32> indices;
    for (const auto &[matId, matIdxList] : materialIndices)
    {
        app::SubMesh subMesh{};
        subMesh.materialId = matId;
        subMesh.indexOffset = static_cast<Uint32>(indices.size());
        subMesh.indexCount = static_cast<Uint32>(matIdxList.size());
        result.subMeshes.push_back(subMesh); // Store the Mesh in the member variable

        indices.insert(indices.end(), matIdxList.begin(), matIdxList.end());
    }

    // Normalize the model
    glm::vec3 center = (minBounds + maxBounds) * 0.5f;
    glm::vec3 extents = maxBounds - minBounds;
    float maxExtent = std::max({extents.x, extents.y, extents.z});
    float scale = 2.0f / maxExtent;

    for (auto &v : vertices)
    {
        v.position = (v.position - center) * scale;
    }

    result.vertices = std::move(vertices);
    result.indices  = std::move(indices);

    return result;
}

SDL_GPUTexture* MeshLoader::LoadTexture(SDL_GPUDevice* gpu,
                                       const std::string& path)
{
    int width, height, channels;
    // Force 4 channels (RGBA)
    stbi_uc *pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        std::cerr << "Failed to load texture " << path << '\n';
        return nullptr;
    }

    Uint32 imageSize = width * height * 4;

    // Crete the GPU Texture
    SDL_GPUTextureCreateInfo texInfo{};
    texInfo.type = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.width = static_cast<Uint32>(width);
    texInfo.height = static_cast<Uint32>(height);
    texInfo.layer_count_or_depth = 1;
    texInfo.num_levels = 1;
    texInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER; // This tell SDL that we will sample the texture in a shader
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(gpu, &texInfo);

    // Create Transfer Buffer and Copy Pixels
    SDL_GPUTransferBufferCreateInfo transferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, imageSize};
    SDL_GPUTransferBuffer *transferBuffer = SDL_CreateGPUTransferBuffer(gpu, &transferInfo);

    // Map the buffer, copy the pixels, and free the CPU memory
    void *mapped = SDL_MapGPUTransferBuffer(gpu, transferBuffer, false);
    std::memcpy(mapped, pixels, imageSize);
    SDL_UnmapGPUTransferBuffer(gpu, transferBuffer);
    stbi_image_free(pixels);

    // Upload to GPU texture using a copy pass
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu);
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = transferBuffer;
    source.offset = 0;

    SDL_GPUTextureRegion destination{};
    destination.texture = texture;
    destination.w = static_cast<Uint32>(width);
    destination.h = static_cast<Uint32>(height);
    destination.d = 1;

    SDL_UploadToGPUTexture(copyPass, &source, &destination, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(gpu, transferBuffer);

    return texture;
}