#include "RenderApp.h"

#include <unordered_map>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Custom function to find equal hashes

RenderApp::~RenderApp()
{
    Cleanup();
}

bool RenderApp::Initialize()
{
    if (!ctx.Initialize("3D Renderer", app::WIDTH, app::HEIGHT))
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

bool RenderApp::CreateMesh()
{
    // 1. Delegate loading OBJ, deduplicating vertices, and textures to MeshLoader
    LoadedMesh mesh = MeshLoader::LoadOBJ(
        ctx.GetDevice(),
        "resources/breakfast_room/breakfast_room.obj",
        "resources/breakfast_room/");

    if (mesh.vertices.empty())
    {
        std::cerr << "Failed to load mesh or mesh has no vertices!\n";
        return false;
    }

    // 2. Keep ownership of submeshes and textures for rendering
    subMeshes = std::move(mesh.subMeshes);
    materialTextures = std::move(mesh.textures);

    // 3. Vertex Buffer setup
    vertexCount = mesh.vertices.size();
    size_t vertexBufferSize = vertexCount * sizeof(app::Vertex);

    SDL_GPUBufferCreateInfo vertexBufferInfo{SDL_GPU_BUFFERUSAGE_VERTEX, (Uint32)vertexBufferSize};
    vertexBuffer = SDL_CreateGPUBuffer(ctx.GetDevice(), &vertexBufferInfo);

    SDL_GPUTransferBufferCreateInfo vertexTransferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, (Uint32)vertexBufferSize};
    SDL_GPUTransferBuffer *vertexTransferBuffer = SDL_CreateGPUTransferBuffer(ctx.GetDevice(), &vertexTransferInfo);

    void *vertexMapPtr = SDL_MapGPUTransferBuffer(ctx.GetDevice(), vertexTransferBuffer, false);
    std::memcpy(vertexMapPtr, mesh.vertices.data(), vertexBufferSize);
    SDL_UnmapGPUTransferBuffer(ctx.GetDevice(), vertexTransferBuffer);

    // 4. Index Buffer setup
    indexCount = mesh.indices.size();
    size_t indexBufferSize = indexCount * sizeof(Uint32);

    SDL_GPUBufferCreateInfo indexBufferInfo{SDL_GPU_BUFFERUSAGE_INDEX, (Uint32)indexBufferSize};
    indexBuffer = SDL_CreateGPUBuffer(ctx.GetDevice(), &indexBufferInfo);

    SDL_GPUTransferBufferCreateInfo indicesTransferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, (Uint32)indexBufferSize};
    SDL_GPUTransferBuffer *indicesTransferBuffer = SDL_CreateGPUTransferBuffer(ctx.GetDevice(), &indicesTransferInfo);

    void *indicesMapPtr = SDL_MapGPUTransferBuffer(ctx.GetDevice(), indicesTransferBuffer, false);
    std::memcpy(indicesMapPtr, mesh.indices.data(), indexBufferSize);
    SDL_UnmapGPUTransferBuffer(ctx.GetDevice(), indicesTransferBuffer);

    // 5. Upload both Buffers to GPU
    SDL_GPUCommandBuffer *uploadCmdBuf = SDL_AcquireGPUCommandBuffer(ctx.GetDevice());
    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);

    SDL_GPUTransferBufferLocation vertexSource = {vertexTransferBuffer, 0};
    SDL_GPUBufferRegion vertexDestination = {vertexBuffer, 0, (Uint32)vertexBufferSize};
    SDL_UploadToGPUBuffer(copyPass, &vertexSource, &vertexDestination, false);

    SDL_GPUTransferBufferLocation indicesSource = {indicesTransferBuffer, 0};
    SDL_GPUBufferRegion indicesDestination = {indexBuffer, 0, (Uint32)indexBufferSize};
    SDL_UploadToGPUBuffer(copyPass, &indicesSource, &indicesDestination, false);

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmdBuf);

    SDL_ReleaseGPUTransferBuffer(ctx.GetDevice(), vertexTransferBuffer);
    SDL_ReleaseGPUTransferBuffer(ctx.GetDevice(), indicesTransferBuffer);

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

    vertexShader = SDL_CreateGPUShader(ctx.GetDevice(), &vertInfo);

    SDL_GPUShaderCreateInfo fragInfo = vertInfo;
    fragInfo.num_uniform_buffers = 1;
    fragInfo.num_samplers = 1;
    fragInfo.entrypoint = "fragment_main";
    fragInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;

    fragmentShader = SDL_CreateGPUShader(ctx.GetDevice(), &fragInfo);

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
    colorTargetDesc[0].format = SDL_GetGPUSwapchainTextureFormat(ctx.GetDevice(), ctx.GetWindow());
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

    pipeline = SDL_CreateGPUGraphicsPipeline(ctx.GetDevice(), &pipelineInfo);

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
    whiteFallbackTexture = SDL_CreateGPUTexture(ctx.GetDevice(), &fallbackInfo);

    SDL_GPUTransferBufferCreateInfo fallbackTransferInfo{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, 4};
    SDL_GPUTransferBuffer *fallbackTransfer = SDL_CreateGPUTransferBuffer(ctx.GetDevice(), &fallbackTransferInfo);

    // 4 bytes: R=255, G=255, B=255, A=255 (Solid White)
    Uint8 whitePixel[4] = {255, 255, 255, 255};
    void *map = SDL_MapGPUTransferBuffer(ctx.GetDevice(), fallbackTransfer, false);
    std::memcpy(map, whitePixel, 4);
    SDL_UnmapGPUTransferBuffer(ctx.GetDevice(), fallbackTransfer);

    SDL_GPUCommandBuffer *fallbackCmd = SDL_AcquireGPUCommandBuffer(ctx.GetDevice());
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
    SDL_ReleaseGPUTransferBuffer(ctx.GetDevice(), fallbackTransfer);

    sampler = SDL_CreateGPUSampler(ctx.GetDevice(), &samplerInfo);

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
        else if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
        {
            mouseCaptured = false;
            SDL_SetWindowRelativeMouseMode(ctx.GetWindow(), false);
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT)
        {
            if (!mouseCaptured)
            {
                mouseCaptured = true;
                SDL_SetWindowRelativeMouseMode(ctx.GetWindow(), true);
            }
        }
        else if (event.type == SDL_EVENT_MOUSE_MOTION && mouseCaptured)
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
            SDL_ReleaseGPUTexture(ctx.GetDevice(), depthTexture);

        // Create the new Texture based on the newly resized dimentions of the screen
        SDL_GPUTextureCreateInfo depthTextureInfo{};
        depthTextureInfo.type = SDL_GPU_TEXTURETYPE_2D;
        depthTextureInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        depthTextureInfo.width = width;
        depthTextureInfo.height = height;
        depthTextureInfo.layer_count_or_depth = 1;
        depthTextureInfo.num_levels = 1;
        depthTextureInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;

        depthTexture = SDL_CreateGPUTexture(ctx.GetDevice(), &depthTextureInfo);
        currentDepthWidth = width;
        currentDepthHeight = height;
    }
}

void RenderApp::RenderFrame()
{
    SDL_GPUCommandBuffer *cmdBuf = SDL_AcquireGPUCommandBuffer(ctx.GetDevice());
    if (!cmdBuf)
        return;

    SDL_GPUTexture *swapchainTexture = nullptr;
    Uint32 swapchainWidth = 0, swapchainHeight = 0;

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdBuf, ctx.GetWindow(), &swapchainTexture, &swapchainWidth, &swapchainHeight))
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

    if (ctx.GetDevice())
        SDL_WaitForGPUIdle(ctx.GetDevice());

    for (SDL_GPUTexture *tex : materialTextures)
    {
        if (tex != nullptr)
            SDL_ReleaseGPUTexture(ctx.GetDevice(), tex);
    }
    materialTextures.clear();
    if (whiteFallbackTexture)
        SDL_ReleaseGPUTexture(ctx.GetDevice(), whiteFallbackTexture);

    if (sampler)
        SDL_ReleaseGPUSampler(ctx.GetDevice(), sampler);
    if (depthTexture)
        SDL_ReleaseGPUTexture(ctx.GetDevice(), depthTexture);
    if (vertexBuffer)
        SDL_ReleaseGPUBuffer(ctx.GetDevice(), vertexBuffer);
    if (indexBuffer)
        SDL_ReleaseGPUBuffer(ctx.GetDevice(), indexBuffer);
    if (pipeline)
        SDL_ReleaseGPUGraphicsPipeline(ctx.GetDevice(), pipeline);
    if (vertexShader)
        SDL_ReleaseGPUShader(ctx.GetDevice(), vertexShader);
    if (fragmentShader)
        SDL_ReleaseGPUShader(ctx.GetDevice(), fragmentShader);
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