#include "RenderApp.h"

#include <unordered_map>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Custom function to find equal hashes
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

RenderApp::~RenderApp()
{
    Cleanup();
}

bool RenderApp::Initialize()
{
    if (!InitSDL())
        return false;
    if (!CreateMesh())
        return false;
    if (!CreatePipeline())
        return false;

    // Set Light defaults
    mainLight.lightDirection = glm::vec4(1.0f, -1.0f, 1.0f, 0.0f);
    mainLight.lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    return true;
}

bool RenderApp::InitSDL()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << '\n';
        return false;
    }

    window = SDL_CreateWindow("3D Renderer", app::WIDTH, app::HEIGHT, SDL_WINDOW_RESIZABLE);
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

SDL_GPUTexture *RenderApp::LoadTexture(const std::string &path)
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

    // Map the buffer, copy the pixels, and free the CPU memory'
    void *mapped = SDL_MapGPUTransferBuffer(gpu, transferBuffer, false);
    std::memcpy(mapped, pixels, imageSize);
    SDL_UnmapGPUTransferBuffer(gpu, transferBuffer);
    stbi_image_free(pixels);

    // Upload to GPU
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

bool RenderApp::CreateMesh()
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "resources/breakfast_room/breakfast_room.obj", "resources/breakfast_room/"))
    {
        std::cerr << "Failed to load OBJ " << warn << err << '\n';
    }

    materialTextures.resize(materials.size(), nullptr);
    for (size_t i = 0; i < materials.size(); i++)
    {
        if (!materials[i].diffuse_texname.empty())
        {
            std::string texPath = "resources/breakfast_room/" + materials[i].diffuse_texname;
            materialTextures[i] = LoadTexture(texPath);
            if (materialTextures[i] != nullptr)
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
        subMeshes.push_back(subMesh); // Store the Mesh in the member variable

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

    // Vertex Buffer setup
    vertexCount = vertices.size();
    size_t vertexBufferSize = vertexCount * sizeof(app::Vertex);

    SDL_GPUBufferCreateInfo vertexBufferInfo{SDL_GPU_BUFFERUSAGE_VERTEX, (Uint32)vertexBufferSize};
    vertexBuffer = SDL_CreateGPUBuffer(gpu, &vertexBufferInfo);

    SDL_GPUTransferBufferCreateInfo vertexTransferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, (Uint32)vertexBufferSize};
    SDL_GPUTransferBuffer *vertexTransferBuffer = SDL_CreateGPUTransferBuffer(gpu, &vertexTransferInfo);

    void *vertexMapPtr = SDL_MapGPUTransferBuffer(gpu, vertexTransferBuffer, false);
    std::memcpy(vertexMapPtr, vertices.data(), vertexBufferSize);
    SDL_UnmapGPUTransferBuffer(gpu, vertexTransferBuffer);

    // Index Buffer setup
    indexCount = indices.size();
    size_t indexBufferSize = indexCount * sizeof(Uint32);

    SDL_GPUBufferCreateInfo indexBufferInfo{SDL_GPU_BUFFERUSAGE_INDEX, (Uint32)indexBufferSize};
    indexBuffer = SDL_CreateGPUBuffer(gpu, &indexBufferInfo);

    SDL_GPUTransferBufferCreateInfo indicesTransferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, (Uint32)indexBufferSize};
    SDL_GPUTransferBuffer *indicesTransferBuffer = SDL_CreateGPUTransferBuffer(gpu, &indicesTransferInfo);

    void *indicesMapPtr = SDL_MapGPUTransferBuffer(gpu, indicesTransferBuffer, false);
    std::memcpy(indicesMapPtr, indices.data(), indexBufferSize);
    SDL_UnmapGPUTransferBuffer(gpu, indicesTransferBuffer);

    // Upload both Buffers to GPU
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

    return true;
}

bool RenderApp::CreatePipeline()
{
    std::vector<char> shaderCode = LoadShaderFile("shaders/triangle.msl");
    if (shaderCode.empty())
        return false;

    const size_t shaderSize = shaderCode.size() - 1;

    SDL_GPUShaderCreateInfo vertInfo{};
    vertInfo.num_uniform_buffers = 1;
    vertInfo.code = reinterpret_cast<const Uint8 *>(shaderCode.data());
    vertInfo.code_size = shaderSize;
    vertInfo.entrypoint = "vertex_main";
    vertInfo.format = SDL_GPU_SHADERFORMAT_MSL;
    vertInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;

    vertexShader = SDL_CreateGPUShader(gpu, &vertInfo);

    SDL_GPUShaderCreateInfo fragInfo = vertInfo;
    fragInfo.num_uniform_buffers = 1;
    fragInfo.num_samplers = 1;
    fragInfo.entrypoint = "fragment_main";
    fragInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;

    fragmentShader = SDL_CreateGPUShader(gpu, &fragInfo);

    if (!vertexShader || !fragmentShader)
        return false;

    SDL_GPUVertexBufferDescription vertexBufferDesc[1] = {
        {0, sizeof(app::Vertex)}};

    SDL_GPUVertexAttribute vertexAttributes[4] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(app::Vertex, position)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(app::Vertex, normal)},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(app::Vertex, color)},
        {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(app::Vertex, uv)}};

    SDL_GPUColorTargetDescription colorTargetDesc[1]{};
    colorTargetDesc[0].format = SDL_GetGPUSwapchainTextureFormat(gpu, window);
    colorTargetDesc[0].blend_state.color_write_mask = 0xF;

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDesc;
    pipelineInfo.vertex_input_state.num_vertex_attributes = 4;
    pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

    pipelineInfo.target_info.num_color_targets = 1;
    pipelineInfo.target_info.color_target_descriptions = colorTargetDesc;

    pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    pipelineInfo.depth_stencil_state.enable_depth_test = true;
    pipelineInfo.depth_stencil_state.enable_depth_write = true;
    pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    pipelineInfo.target_info.has_depth_stencil_target = true;
    pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    pipeline = SDL_CreateGPUGraphicsPipeline(gpu, &pipelineInfo);

    // Create the Sampler
    SDL_GPUSamplerCreateInfo samplerInfo{};
    samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;

    // Create the white fallback texture
    SDL_GPUTextureCreateInfo fallbackInfo{};
    fallbackInfo.type = SDL_GPU_TEXTURETYPE_2D;
    fallbackInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    fallbackInfo.width = 1;
    fallbackInfo.height = 1;
    fallbackInfo.layer_count_or_depth = 1;
    fallbackInfo.num_levels = 1;
    fallbackInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    whiteFallbackTexture = SDL_CreateGPUTexture(gpu, &fallbackInfo);

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
    fallbackDest.texture = whiteFallbackTexture;
    fallbackDest.w = 1;
    fallbackDest.h = 1;
    fallbackDest.d = 1;

    SDL_UploadToGPUTexture(fallbackCopy, &fallbackSource, &fallbackDest, false);
    SDL_EndGPUCopyPass(fallbackCopy);
    SDL_SubmitGPUCommandBuffer(fallbackCmd);
    SDL_ReleaseGPUTransferBuffer(gpu, fallbackTransfer);

    sampler = SDL_CreateGPUSampler(gpu, &samplerInfo);

    return pipeline != nullptr && sampler != nullptr;
}

void RenderApp::Run()
{

    bool running = true;
    lastFrameTime = SDL_GetTicks();

    while (running)
    {
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastFrameTime) / 1000.0f;
        lastFrameTime = currentTime;

        HandleEvents(running);
        camera.ProcessKeyboard(SDL_GetKeyboardState(nullptr), deltaTime);
        RenderFrame();
    }
}

void RenderApp::HandleEvents(bool &running)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            running = false;
        }
        else if(event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
        {
            mouseCaptured = false;
            SDL_SetWindowRelativeMouseMode(window, false);
        }
        else if(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT)
        {
            if(!mouseCaptured)
            {
                mouseCaptured = true;
                SDL_SetWindowRelativeMouseMode(window, true);
            }
        }
        else if(event.type == SDL_EVENT_MOUSE_MOTION && mouseCaptured)
        {
            camera.ProcessMouseMovement(event.motion.xrel, event.motion.yrel);
        }
    }
}

void RenderApp::UpdateDepthTexture(Uint32 width, Uint32 height)
{

    if (depthTexture == nullptr || currentDepthWidth != width || currentDepthHeight != height)
    {
        if (depthTexture)
            SDL_ReleaseGPUTexture(gpu, depthTexture);

        // Create the new Texture based on the newly resized dimentions of the screen
        SDL_GPUTextureCreateInfo depthTextureInfo{};
        depthTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        depthTextureInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        depthTextureInfo.width = width;
        depthTextureInfo.height = height;
        depthTextureInfo.layer_count_or_depth = 1;
        depthTextureInfo.num_levels = 1;
        depthTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        depthTexture = SDL_CreateGPUTexture(gpu, &depthTextureInfo);
        currentDepthWidth = width;
        currentDepthHeight = height;
    }
}

void RenderApp::RenderFrame()
{
    SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(gpu);
    if (!cmdBuf)
        return;

    SDL_GPUTexture *swapchainTexture = nullptr;
    Uint32 swapchainWidth = 0, swapchainHeight = 0;

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuf, window, &swapchainTexture, &swapchainWidth, &swapchainHeight))
    {

        SDL_SubmitGPUCommandBuffer(cmdBuf);
        return;
    }

    if (swapchainTexture != nullptr)
    {
        UpdateDepthTexture(swapchainWidth, swapchainHeight);

        float time = SDL_GetTicks() / 1000.0f;
        float aspect = swapchainHeight > 0 ? (float)swapchainWidth / (float)swapchainHeight : 1.0f;

        // Math behind MVP Matrix
        glm::mat4 proj = camera.GetProjectionMatrix(aspect);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 world = glm::mat4(1.0f);
        glm::mat4 mvp = proj * view * world;

        SDL_GPUColorTargetInfo colorTargetInfo{};
        colorTargetInfo.texture = swapchainTexture;
        colorTargetInfo.clear_color = SDL_FColor{0.1f, 0.1f, 0.1f, 1.0f};
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depthTargetInfo{};
        depthTargetInfo.texture = depthTexture;
        depthTargetInfo.clear_depth = 1.0f;
        depthTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        depthTargetInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depthTargetInfo.cycle = true;
        depthTargetInfo.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthTargetInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(cmdBuf, &colorTargetInfo, 1, &depthTargetInfo);
        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

        // Bind the vertexBuffer
        SDL_GPUBufferBinding vertexBinding{vertexBuffer, 0};
        SDL_BindGPUVertexBuffers(renderPass, 0, &vertexBinding, 1);

        // Bind the indexBuffer
        SDL_GPUBufferBinding indexBinding{indexBuffer, 0};
        SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        app::VertexUniforms vertData{};
        vertData.mvp = mvp;
        vertData.model = world;

        SDL_PushGPUVertexUniformData(cmdBuf, 0, &vertData, sizeof(app::VertexUniforms));
        SDL_PushGPUFragmentUniformData(cmdBuf, 0, &mainLight, sizeof(app::FragmentUniforms));

        for (const auto &sub : subMeshes)
        {
            SDL_GPUTexture *texToBind = whiteFallbackTexture;
            if (sub.materialId >= 0 && sub.materialId < materialTextures.size() && materialTextures[sub.materialId] != nullptr)
            {
                texToBind = materialTextures[sub.materialId];
            }

            SDL_GPUTextureSamplerBinding texBinding{};
            texBinding.texture = texToBind;
            texBinding.sampler = sampler;

            SDL_BindGPUFragmentSamplers(renderPass, 0, &texBinding, 1);

            // Draw only the indices belonging to this material
            SDL_DrawGPUIndexedPrimitives(renderPass, sub.indexCount, 1, sub.indexOffset, 0, 0);
        }

        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmdBuf);
}

void RenderApp::Cleanup()
{
    if (gpu)
        SDL_WaitForGPUIdle(gpu);

    for (SDL_GPUTexture *tex : materialTextures)
    {
        if (tex != nullptr)
            SDL_ReleaseGPUTexture(gpu, tex);
    }
    materialTextures.clear();
    if(whiteFallbackTexture)                            
        SDL_ReleaseGPUTexture(gpu, whiteFallbackTexture);

    if (sampler)
        SDL_ReleaseGPUSampler(gpu, sampler);
    if (depthTexture)
        SDL_ReleaseGPUTexture(gpu, depthTexture);
    if (vertexBuffer)
        SDL_ReleaseGPUBuffer(gpu, vertexBuffer);
    if (indexBuffer)
        SDL_ReleaseGPUBuffer(gpu, indexBuffer);
    if (pipeline)
        SDL_ReleaseGPUGraphicsPipeline(gpu, pipeline);
    if (vertexShader)
        SDL_ReleaseGPUShader(gpu, vertexShader);
    if (fragmentShader)
        SDL_ReleaseGPUShader(gpu, fragmentShader);

    if (gpu && window)
        SDL_ReleaseWindowFromGPUDevice(gpu, window);
    if (gpu)
        SDL_DestroyGPUDevice(gpu);
    if (window)
        SDL_DestroyWindow(window);

    SDL_Quit();

    gpu = nullptr;
    window = nullptr;
}

std::vector<char> RenderApp::LoadShaderFile(const std::string &path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open shader file: " << path << '\n';
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize + 1);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    buffer[fileSize] = '\0';
    return buffer;
}