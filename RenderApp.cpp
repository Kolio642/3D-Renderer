#include "RenderApp.h"

#include <unordered_map>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Internal Constants & Structs
namespace app
{
    const int WIDTH = 800;
    const int HEIGHT = 600;
    const float CAMERA_DISTANCE = 4.0f;
    const float CAMERA_HEIGHT = 1.5f;
    const float WORLD_SPIN_SPEED = 0.8f;

    struct Vertex
    {
        glm::vec3 position;
        glm::vec4 color;
    };
}

// Custom Hasher for tinyobj::index_t as a map key
struct IndexHasher
{
    size_t operator()(const tinyobj::index_t &index) const
    {
        return (((size_t)index.normal_index) ^
                ((size_t)index.texcoord_index << 1) ^
                ((size_t)index.vertex_index << 2));
    }
};

// Custom function to find equal hashes
struct IndexEqual
{
    bool operator()(const tinyobj::index_t &a, const tinyobj::index_t &b) const
    {
        return a.vertex_index == b.vertex_index &&
               a.normal_index == b.normal_index &&
               a.texcoord_index == b.texcoord_index;
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

bool RenderApp::CreateMesh()
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "recources/dragon.obj"))
    {
        std::cerr << "Failed to load OBJ " << warn << err << '\n';
    }

    std::vector<app::Vertex> vertices;

    // Center and scale the model
    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::min());

    std::vector<Uint32> indices; // Instructions how to draw the points

    std::unordered_map<tinyobj::index_t, Uint32, IndexHasher, IndexEqual> uniqueVertices;

    for (const auto &shape : shapes)
    {
        for (const auto &index : shape.mesh.indices)
        {
            // If we haven't seen this vertex add it to the create one
            if (uniqueVertices.count(index) == 0)
            {
                app::Vertex vertex{};
                vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]};

                // Map normals to RGB colors
                if (index.normal_index >= 0)
                {
                    vertex.color = {
                        (attrib.normals[3 * index.normal_index + 0] + 1.0f) * 0.5f,
                        (attrib.normals[3 * index.normal_index + 1] + 1.0f) * 0.5f,
                        (attrib.normals[3 * index.normal_index + 2] + 1.0f) * 0.5f,
                        1.0f};
                }
                else
                {
                    // Fallback color if the OBJ has no normals
                    vertex.color = {0.8f, 0.8f, 0.8f, 1.0f};
                }

                minBounds = glm::min(minBounds, vertex.position);
                maxBounds = glm::max(maxBounds, vertex.position);

                // Add new vertex to array
                vertices.push_back(vertex);

                // Record it's location in the hash map
                uniqueVertices[index] = static_cast <Uint32> (vertices.size() - 1);
            }

            indices.push_back(uniqueVertices[index]);
        }
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
    SDL_GPUTransferBuffer* vertexTransferBuffer = SDL_CreateGPUTransferBuffer(gpu, &vertexTransferInfo);

    void* vertexMapPtr = SDL_MapGPUTransferBuffer(gpu, vertexTransferBuffer, false);
    std::memcpy(vertexMapPtr, vertices.data(), vertexBufferSize);
    SDL_UnmapGPUTransferBuffer(gpu, vertexTransferBuffer);

    // Index Buffer setup
    indexCount = indices.size();
    size_t indexBufferSize = indexCount * sizeof(Uint32);

    SDL_GPUBufferCreateInfo indexBufferInfo{SDL_GPU_BUFFERUSAGE_INDEX, (Uint32)indexBufferSize};
    indexBuffer = SDL_CreateGPUBuffer(gpu, &indexBufferInfo);

    SDL_GPUTransferBufferCreateInfo indicesTransferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, (Uint32)indexBufferSize};
    SDL_GPUTransferBuffer* indicesTransferBuffer = SDL_CreateGPUTransferBuffer(gpu, &indicesTransferInfo);

    void* indicesMapPtr = SDL_MapGPUTransferBuffer(gpu, indicesTransferBuffer, false);
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
    fragInfo.num_uniform_buffers = 0;
    fragInfo.entrypoint = "fragment_main";
    fragInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;

    fragmentShader = SDL_CreateGPUShader(gpu, &fragInfo);

    if (!vertexShader || !fragmentShader)
        return false;

    SDL_GPUVertexBufferDescription vertexBufferDesc[1] = {
        {0, sizeof(app::Vertex)}};

    SDL_GPUVertexAttribute vertexAttributes[2] = {
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(app::Vertex, position)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(app::Vertex, color)}};

    SDL_GPUColorTargetDescription colorTargetDesc[1]{};
    colorTargetDesc[0].format = SDL_GetGPUSwapchainTextureFormat(gpu, window);
    colorTargetDesc[0].blend_state.color_write_mask = 0xF;

    SDL_GPUGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.vertex_shader = vertexShader;
    pipelineInfo.fragment_shader = fragmentShader;
    pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
    pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDesc;
    pipelineInfo.vertex_input_state.num_vertex_attributes = 2;
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
    return pipeline != nullptr;
}

void RenderApp::Run()
{

    bool running = true;

    while (running)
    {
        HandleEvents(running);
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

        // Math behind MVP M<atrix
        glm::mat4 proj = glm::perspectiveZO(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, app::CAMERA_HEIGHT, app::CAMERA_DISTANCE), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 world = glm::rotate(glm::mat4(1.0f), time * app::WORLD_SPIN_SPEED, glm::vec3(0.0f, 1.0f, 0.0f));
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
        SDL_GPUBufferBinding indexBinding {indexBuffer, 0};
        SDL_BindGPUIndexBuffer(renderPass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        SDL_PushGPUVertexUniformData(cmdBuf, 0, glm::value_ptr(mvp), sizeof(glm::mat4));

        SDL_DrawGPUIndexedPrimitives(renderPass, indexCount, 1, 0, 0, 0);

        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmdBuf);
}

void RenderApp::Cleanup()
{
    if (gpu)
        SDL_WaitForGPUIdle(gpu);

    if (depthTexture)
        SDL_ReleaseGPUTexture(gpu, depthTexture);
    if (vertexBuffer)
        SDL_ReleaseGPUBuffer(gpu, vertexBuffer);
    if(indexBuffer)
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