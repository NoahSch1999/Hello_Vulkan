#include <iostream>
#include <vector>

#include <SDL3/SDL_vulkan.h>

#include "aZeroWindow.hpp"
#include "Loading_Assets/Gltf.hpp"
#include "Loading_Assets/ImageLoading.hpp"
#include "Loading_Assets/AssetPackage.hpp"

#include "render_api/Core.hpp"
#include "render_api/Swapchain.hpp"
#include "render_api/DepthStencilImage.hpp"
#include "render_api/Buffer.hpp"
#include "render_api/CommandPool.hpp"
#include "render_api/Sampler.hpp"
#include "render_api/Descriptor.hpp"
#include "render_api/Fence.hpp"
#include "render_api/Semaphore.hpp"
#include "render_api/CommandQueue.hpp"
#include "render_api/Compiler.hpp"
#include "render_api/RenderPass.hpp"

/*
Ideas:
- assets as components with flecs
- 
*/

struct CameraData
{
    glm::mat4 ViewMatrix;
    glm::mat4 ViewProjectionMatrix;
    aZero::BoundingVolume::BoundingFrustum Frustum;
};

struct IndirectArgumentConstantData
{
    glm::mat4 Instance;
    uint32_t GlobalMeshletOffset;
    uint32_t GlobalVertexOffset;
    uint32_t MaterialIndex;
    uint32_t MeshletCount;
};

struct IndirectArguments
{
    IndirectArgumentConstantData Data;
    uint32_t GroupX;
    uint32_t GroupY;
    uint32_t GroupZ;
};

struct PushConstants
{
    // CS CULL => AS
    IndirectArgumentConstantData IA_Args;

    // AS, MS, PS
    uint64_t CameraAddress;

    // AS
    uint64_t MeshletBoundBufferAddress;

    // MS
    uint64_t MeshletBufferAddress;
    uint64_t VertexBufferAddress;

    uint64_t MaterialBufferAddress;

    uint32_t RenderMode;
};

class RenderWindow : public aZero::Window::Window_Win32 {
public:
    RenderWindow() = default;
    explicit RenderWindow(const aZero::Window::WindowDesc& desc)
        :Window_Win32(desc) { }

    aZero::RenderAPI::Core* m_Core;
    aZero::RenderAPI::Swapchain* m_Swapchain;
    aZero::RenderAPI::DepthStencilImage* m_DsImage;
    aZero::RenderAPI::ImageView* m_Dsv;

private:
    virtual void PollEventImpl(const SDL_Event& event) override {
        if (GetAsyncKeyState(VK_ESCAPE)) {
            this->Close();
        }

        if (event.type == SDL_EVENT_WINDOW_RESIZED)
        {
            m_Swapchain->Resize(event.window.data1, event.window.data2);

            // todo Resize dsv and other stuff
            aZero::RenderAPI::DepthStencilImage::Desc depthImageDesc{
                .Width = m_Swapchain->GetLastCreateInfo().imageExtent.width,
                .Height = m_Swapchain->GetLastCreateInfo().imageExtent.height
            };

            *m_DsImage = aZero::RenderAPI::DepthStencilImage(*m_Core, depthImageDesc);
            m_DsImage->Get().Transition({
                .DstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .DstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .NewLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1 }
                });

            aZero::RenderAPI::ImageView::Desc depthImageViewDesc{
                .Image = &m_DsImage->Get(),
                .ImageAspect = aZero::RenderAPI::EImageAspect::DepthStencil
            };
            *m_Dsv = aZero::RenderAPI::ImageView(*m_Core, depthImageViewDesc);
        }
    }
};

aZero::Asset::LegacyMeshData meshData;
aZero::Asset::MeshletMeshData meshletMeshData;
aZero::Asset::ImageData imageData;
bool LoadAssets(const std::filesystem::path& path)
{
    static constexpr auto supportedExtensions =
        fastgltf::Extensions::KHR_mesh_quantization |
        fastgltf::Extensions::KHR_texture_transform |
        fastgltf::Extensions::KHR_materials_variants |
        fastgltf::Extensions::KHR_lights_punctual |
        fastgltf::Extensions::KHR_texture_basisu
        ;

    fastgltf::Parser parser(supportedExtensions);

    constexpr auto gltfOptions =
        fastgltf::Options::DontRequireValidAssetMember |
        fastgltf::Options::AllowDouble |
        fastgltf::Options::LoadExternalBuffers |
        //fastgltf::Options::LoadExternalImages |
        fastgltf::Options::GenerateMeshIndices |
        fastgltf::Options::DecomposeNodeMatrices
        ;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
    if (!bool(gltfFile)) {
        std::cerr << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
        return false;
    }

    auto loadedAsset = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
    fastgltf::Asset* asset = loadedAsset.get_if();
    if (!asset) {
        std::cerr << "Failed to load glTF file: " << fastgltf::getErrorMessage(loadedAsset.error()) << '\n';
        return false;
    }

    meshData = aZero::File::ExtractGltfLegacyMesh(asset->meshes[0], asset);
    meshletMeshData = aZero::File::ExtractGltfMeshletMesh(asset->meshes[0], asset);
    imageData = aZero::File::ExtractGltfImage(asset->images[0], asset, path);
    return true;
}

int main()
{
    using namespace aZero;

    bool loaded = LoadAssets("C:/Projects/Programming/Hello_Vulkan/KtxCube.gltf");
    if (!loaded) {
        std::cout << "Failed to load assets\n";
        exit(-1);
    }

    Package::AssetPackage assetPac("C:/Projects/Programming/Hello_Vulkan/MyPac.pac");

    aZero::Asset::MeshletMeshData meshletMeshDataTemp = assetPac.GetAsset<aZero::Asset::MeshletMeshData>(0);
    imageData = assetPac.GetAsset<aZero::Asset::ImageData>(0);

    aZero::Window::Init(SDL_INIT_VIDEO);

    SDL_Vulkan_LoadLibrary(NULL);

    uint32_t sdlExtensionCount = 0;
    char const* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    std::vector<const char*> extensions;
    extensions.reserve(sdlExtensionCount + 1);
    for (uint32_t i = 0; i < sdlExtensionCount; i++)
    {
        extensions.push_back(sdlExtensions[i]);
    }

    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    RenderAPI::Core::Desc coreDesc {
        .ApplicationName = "Hello Vulkan",
        .Extensions = extensions,
        .EnableValidationLayer = true
    };
    RenderAPI::Core core(coreDesc);

    // ----------------------------------------------------------------------------------------------------
    // create and compile slang shader
    Compiler::Compiler compiler(core);
    Compiler::CompilerSession::Desc d;
    d.CompilerOptions = std::vector<slang::CompilerOptionEntry>({
        {
            slang::CompilerOptionName::EmitSpirvDirectly,
            {slang::CompilerOptionValueKind::Int, 1}
        },
        {
            slang::CompilerOptionName::DebugInformation,
            {slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_STANDARD}
        },
        {
            slang::CompilerOptionName::Optimization,
            {slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_NONE}
        }
        });
    d.PreprocessorMacros.push_back({ "HELLO", "" });

    Compiler::CompilerSession session = compiler.CreateSession(d).value();

    Compiler::ShaderModule moduleTaskShaderPass = session.LoadModule("C:/Projects/Programming/Hello_Vulkan/Shaders/IA_Pipeline/Geo_TaskShader.slang").value();
    Compiler::ShaderModule moduleMeshShaderPass = session.LoadModule("C:/Projects/Programming/Hello_Vulkan/Shaders/IA_Pipeline/Geo_MeshShader.slang").value();
    Compiler::ShaderModule modulePixelShaderPass = session.LoadModule("C:/Projects/Programming/Hello_Vulkan/Shaders/IA_Pipeline/Geo_PixelShader.slang").value();

    Compiler::ShaderProgram::MeshletDesc msProgramDesc{
        .EntryPoints = {"asMain", "msMain", "psMain"},
        .Modules = {&moduleTaskShaderPass, &moduleMeshShaderPass, &modulePixelShaderPass}
    };
    Compiler::ShaderProgram msProgram(msProgramDesc);

    Compiler::ShaderModule module = session.LoadModule("C:/Projects/Programming/Hello_Vulkan/Shaders/pixel.slang").value();
    Compiler::ShaderProgram::VertexDesc vsProgramDesc{
        .EntryPoints = {"vsMain", "psMain"},
        .Modules = {&module}
    };
    Compiler::ShaderProgram program(vsProgramDesc);

    // ----------------------------------------------------------------------------------------------------

    RenderAPI::CommandQueue graphicsQueue(core, RenderAPI::EQueueType::Graphics);

    // setup window and surface (thanks aZeroWindow <3)
    RenderWindow window(aZero::Window::WindowDesc("Hello Vulkan", aZero::Window::WindowDesc::Rect({ 0, 0, 1920, 1080 }), { 0,0,0,0 }, SDL_WINDOW_RESIZABLE, {}));

    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window.GetSDLWindow(), core.GetInstance(), nullptr, &surface);

    RenderAPI::Swapchain::Desc swapChainDesc{
        .Surface = surface,
        .Format = RenderAPI::EImageFormat::BGRA8_UNORM_SRGB,
        .FramesInFlight = 3,
        .Width = (uint32_t)window.GetClientDimensions()[0],
        .Height = (uint32_t)window.GetClientDimensions()[1]
    };

    RenderAPI::Swapchain swapchain(core, swapChainDesc);

    RenderAPI::DepthStencilImage::Desc depthImageDesc{
        .Width = (uint32_t)window.GetClientDimensions()[0],
        .Height = (uint32_t)window.GetClientDimensions()[1]
    };
    RenderAPI::DepthStencilImage depthImage(core, depthImageDesc);

    RenderAPI::ImageView::Desc depthImageViewDesc{
        .Image = &depthImage.Get(),
        .ImageAspect = RenderAPI::EImageAspect::DepthStencil
    };

    RenderAPI::Semaphore sem(core, RenderAPI::ESemaphoreType::Timeline);

    RenderAPI::ImageView depthImageView(core, depthImageViewDesc);

    window.m_Core = &core;
    window.m_Swapchain = &swapchain;
    window.m_DsImage = &depthImage;
    window.m_Dsv = &depthImageView;

    RenderAPI::Buffer::Desc vBufferDesc{
        .Size = sizeof(aZero::Asset::Vertex) * meshData.Vertices.size() + sizeof(aZero::Asset::VertexIndex) * meshData.Indices.size(),
        .UsageBits = RenderAPI::EBufferUsageBits::Vertex | RenderAPI::EBufferUsageBits::Index,
        .MemoryType = RenderAPI::EMemoryType::BestFit,
        .SeperateHeap = false
    };

    RenderAPI::Buffer vBuffer(core, vBufferDesc);

    vBuffer.Upload(meshData.Vertices.data(), sizeof(aZero::Asset::Vertex) * meshData.Vertices.size(), 0);
    vBuffer.Upload(meshData.Indices.data(), sizeof(aZero::Asset::VertexIndex) * meshData.Indices.size(), sizeof(aZero::Asset::Vertex) * meshData.Vertices.size());

    struct ShaderData {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model[3];
        glm::vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
        uint32_t selected{ 1 };
    } shaderData{};

    std::vector<RenderAPI::Buffer> shaderDataBuffers(swapchain.GetFramesInFlight());
    RenderAPI::Buffer::Desc shaderDataBufferDesc{
        .Size = sizeof(ShaderData),
        .UsageBits = RenderAPI::EBufferUsageBits::DeviceAddress,
        .MemoryType = RenderAPI::EMemoryType::BestFit,
        .SeperateHeap = false
    };

    for (auto i = 0; i < shaderDataBuffers.size(); i++) {
        shaderDataBuffers[i] = RenderAPI::Buffer(core, shaderDataBufferDesc);
    }

    std::vector<RenderAPI::Fence> fences(swapchain.GetFramesInFlight());

    for (auto i = 0; i < swapchain.GetFramesInFlight(); i++) {
        fences[i] = RenderAPI::Fence(core, true);
    }

    RenderAPI::CommandPool::Desc cmdPoolDesc{
        .QueueType = RenderAPI::EQueueType::Graphics
    };
    RenderAPI::CommandPool cmdPool(core, cmdPoolDesc);

    std::vector<RenderAPI::CommandBuffer> cmdBuffers = cmdPool.CreateCommandBuffers(core, swapchain.GetFramesInFlight());

    RenderAPI::Image::Desc meshImageDesc{
        .Format = imageData.Format,
        .Type = imageData.Type,
        .Usage = RenderAPI::EImageUsageBits::TransferDst | RenderAPI::EImageUsageBits::Sampled,
        .Width = imageData.BaseWidth, .Height = imageData.BaseHeight, .Depth = imageData.DepthOrArrayCount,
        .MipCount = (uint32_t)imageData.MipLevels.size()
    };

    RenderAPI::Image meshImage(core, meshImageDesc);

    RenderAPI::ImageView::Desc meshImageViewDesc{
        .Image = &meshImage,
        .ImageAspect = RenderAPI::EImageAspect::Color,
        .LayerStart = 0u,
        .LayerCount = 1u,
        .MipStart = 0,
        .MipCount = (uint32_t)imageData.MipLevels.size()
    };

    RenderAPI::ImageView meshImageView(core, meshImageViewDesc);

    RenderAPI::Buffer::Desc imgSrcBufferDesc{
       .Size = (uint32_t)imageData.Data.size(),
       .UsageBits = RenderAPI::EBufferUsageBits::TransferSrc,
       .MemoryType = RenderAPI::EMemoryType::BestFit,
       .SeperateHeap = false
    };

    RenderAPI::Buffer imgSrcBuffer(core, imgSrcBufferDesc);

    imgSrcBuffer.Upload(imageData.Data.data(), imageData.Data.size(), 0);

    std::vector<uint32_t> mipOffsets;
    for (auto& ele : imageData.MipLevels) {
        mipOffsets.push_back(ele.Offset);
    }

    RenderAPI::BufferToImageCopyDesc meshImageUploadDesc{
       .BaseWidth = imageData.BaseWidth,
       .BaseHeight = imageData.BaseHeight,
       .DepthOrArrayCount = imageData.DepthOrArrayCount,
       .MipOffsets = mipOffsets
    };
    meshImage.Upload(core, graphicsQueue, cmdPool, imgSrcBuffer, meshImageUploadDesc);

    RenderAPI::Sampler sampler(core, {});

    RenderAPI::DescriptorSet::Desc setDesc{ .Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .DescriptorCount = core.GetVariadicDescriptorCount(), .ShaderStages = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT};
    RenderAPI::DescriptorPool::Desc descriptorPoolDesc{
        .MaxSets = 1,
        .Sets {setDesc}
    };
    RenderAPI::DescriptorPool descriptorPool(core, descriptorPoolDesc);
    RenderAPI::DescriptorSet descriptorSet = descriptorPool.CreateDescriptorSet(setDesc);
    RenderAPI::DescriptorHandle descriptorHandle{
        .Sampler = &sampler,
        .View = &meshImageView,
        .Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .Index = 0
    };
    descriptorSet.UpdateDescriptor(descriptorHandle);

    // vs pass
    Pipeline::RenderPass renderPass(core);
    Pipeline::RenderPass::VertexPassDesc vpDesc;
    vpDesc.Program = &program;
    vpDesc.DepthDesc = Pipeline::RenderPass::Desc::DepthStencil();
    vpDesc.RenderTargetFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    Pipeline::RenderPass::VertexPassDesc::VertexBinding vBinding{
        .Stride = sizeof(aZero::Asset::Vertex),
        .InputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .AttributeFormats = { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32_UINT, VK_FORMAT_R32_UINT }
    };
    vpDesc.VertexBindings = { vBinding };
    renderPass.Compile(vpDesc);

    // ms pass
    Pipeline::RenderPass msRenderPass(core);
    Pipeline::RenderPass::MeshShaderPassDesc mspDesc;
    mspDesc.Program = &msProgram;
    mspDesc.DepthDesc = Pipeline::RenderPass::Desc::DepthStencil();
    mspDesc.RenderTargetFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    msRenderPass.Compile(mspDesc);

    uint64_t lastTime{ SDL_GetTicks() };
    uint32_t frameIndex = 0;
    //uint32_t imageIndex{ 0 };

    glm::vec3 camPos{ 0.0f, 0.0f, -6.0f };
    glm::vec3 objectRotations[3]{};

    depthImage.Get().Transition({
                .DstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .DstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .NewLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1 }
    });

    RenderAPI::Buffer meshletBuffer, cameraBuffer, meshletBoundsBuffer, vertexBuffer, materialBuffer;
    {
        RenderAPI::Buffer::Desc passBufferDesc{
            .Size = sizeof(1024 * 1024),
            .UsageBits = RenderAPI::EBufferUsageBits::DeviceAddress,
            .MemoryType = RenderAPI::EMemoryType::BestFit,
            .SeperateHeap = false
        };
        passBufferDesc.Name = "cameraBuffer";
        cameraBuffer = RenderAPI::Buffer(core, passBufferDesc);
        passBufferDesc.Name = "meshletBuffer";
        meshletBuffer = RenderAPI::Buffer(core, passBufferDesc);
        passBufferDesc.Name = "meshletBoundsBuffer";
        meshletBoundsBuffer = RenderAPI::Buffer(core, passBufferDesc);
        passBufferDesc.Name = "vertexBuffer";
        vertexBuffer = RenderAPI::Buffer(core, passBufferDesc);
        passBufferDesc.Name = "materialBuffer";
        materialBuffer = RenderAPI::Buffer(core, passBufferDesc);
    }

    meshletBuffer.Upload(meshletMeshData.Meshlets, 0);
    meshletBoundsBuffer.Upload(meshletMeshData.MeshletBounds, 0);
    vertexBuffer.Upload(meshletMeshData.Vertices, 0);

    struct PBRMaterial
    {
        // todo Use the albedo alpha channel for roughness and normal w channel for metallic
        uint32_t AlbedoTexture;
        uint32_t NormalMap;
        uint32_t RoughnessMetallic;
        uint32_t GlowMap;
        uint32_t TransparencyMap;
    };
    PBRMaterial material{
        .AlbedoTexture = 0xffffffff
    };
    materialBuffer.Upload(material, 0);

    while (window.IsOpen())
    {
        window.PollEvents();

        // Wait on fence
        // we use fences since we cant read semaphores on the cpu (its only for gpu-synchronization)
        fences[frameIndex].WaitFor();
        fences[frameIndex].Reset();

        swapchain.NextFrame(frameIndex);

        // Update shader data
        shaderData.projection = glm::perspective(glm::radians(45.0f), (float)window.GetClientDimensions()[0] / (float)window.GetClientDimensions()[1], 0.1f, 32.0f);
        shaderData.view = glm::translate(glm::mat4(1.0f), camPos);

        CameraData cameraData{
            .ViewMatrix = shaderData.view,
            .ViewProjectionMatrix = shaderData.view * shaderData.projection
        };
        cameraBuffer.Upload(cameraData, frameIndex * sizeof(CameraData));

        for (auto i = 0; i < 3; i++) {
            auto instancePos = glm::vec3((float)(i - 1) * 3.0f, 0.0f, 0.0f);
            shaderData.model[i] = glm::translate(glm::mat4(1.0f), instancePos) * glm::mat4_cast(glm::quat(objectRotations[i]));
        }
        shaderDataBuffers[frameIndex].Upload(&shaderData, sizeof(ShaderData), 0);

        // Record command buffer
        RenderAPI::CommandBuffer& cb = cmdBuffers[frameIndex];
        cb.Reset(); // reset (needs to be done after checking that its no longer in use)

        cb.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        VkImageMemoryBarrier2 outputBarriers[2];
        outputBarriers[0] = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                // old usage
                .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // what pipeline stage(s) to wait on
                .srcAccessMask = 0,

                // new usage
                .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // what pipeline stage(s) it will be used on
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

                // how vulkan will treat the image
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, // i dont care about the old layout
                .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, // ready to be used as an attachment. VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL is a combo of all attachment layouts in one enum

                .image = swapchain.GetCurrentBackBuffer(),
                .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
        };

        outputBarriers[1] = depthImage.Get().GetTransition();

        cb.Barrier({}, { outputBarriers[0], outputBarriers[1] }, {});

        msRenderPass.Render(
            cb,
            {
                .ColorTargets = {{.View = swapchain.GetCurrentBackBufferView() }},
                .DepthTarget = {.View = &depthImageView},
                .RenderWidth = static_cast<uint32_t>(window.GetClientDimensions()[0]),
                .RenderHeight = static_cast<uint32_t>(window.GetClientDimensions()[1]),
                .DescriptorSets = {&descriptorSet},
                .Callback = [&](RenderAPI::CommandBuffer& cb, Pipeline::RenderPass& pass) {
                    
                    // since we made them dynamic when creating the pipeline we overwrite them here
                    VkViewport vp{
                        .width = static_cast<float>(window.GetClientDimensions()[0]),
                        .height = static_cast<float>(window.GetClientDimensions()[1]),
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f
                    };
                    VkRect2D scissor{.extent{.width = static_cast<uint32_t>(window.GetClientDimensions()[0]), .height = static_cast<uint32_t>(window.GetClientDimensions()[1]) } };
                    cb.SetScizzorRectsAndViewports({ vp }, { scissor });

                    PushConstants constants{
                        .IA_Args = {
                            .Instance = shaderData.model[0],
                            .GlobalMeshletOffset = 0,
                            .GlobalVertexOffset = 0,
                            .MaterialIndex = 0,
                            .MeshletCount = (uint32_t)meshletMeshData.Meshlets.size()
                        },
                        .CameraAddress = (uint32_t)cameraBuffer.GetDeviceAddress().value() + frameIndex * sizeof(CameraData),
                        .MeshletBoundBufferAddress = (uint32_t)meshletBoundsBuffer.GetDeviceAddress().value(),
                        .MeshletBufferAddress = (uint32_t)meshletBuffer.GetDeviceAddress().value(),
                        .VertexBufferAddress = (uint32_t)vertexBuffer.GetDeviceAddress().value(),
                        .MaterialBufferAddress = (uint32_t)materialBuffer.GetDeviceAddress().value(),
                        .RenderMode = 0u
                    };
                    auto constantsBlock = pass.GetPushConstantBlock("Constants");
                    cb.SetPushConstants(constantsBlock, constants);
                    cb.DispatchMesh(meshletMeshData.Meshlets.size(), 1, 1);
                    /*cb.SetVertexBuffer(vBuffer, 0);
                    cb.SetIndexBuffer(vBuffer, sizeof(aZero::Asset::Vertex) * meshData.Vertices.size());

                    auto pushconst = shaderDataBuffers[frameIndex].GetDeviceAddress().value();
                    auto pcBinding = pass.GetPushConstantBinding("pc", "address");
                    cb.SetPushConstants(pcBinding, pushconst);

                    auto pcBindingFrag = pass.GetPushConstantBinding("pc", "idk");
                    cb.SetPushConstants(pcBindingFrag, 0.2f);

                    cb.DrawIndexed((uint32_t)meshData.Indices.size(), 3, 0, 0, 0);*/
                }
            }
        );

        swapchain.CompleteFrame(cb);

        cb.StopRecording();

        /*
        before we start outputting to the swapchain image we need to guarantee that the current swapchain image isnt in use by the present-engine.
        this is done by waiting on a semaphore (waitSemaphoreInfo + swapchainSemaphores[frameIndex]) that is signalled when vkAcquireNextImageKHR knows that the image isnt in use by the present-engine anymore.
            we wait my using the VkSemaphoreSubmitInfo as an entry into pWaitSemaphoreInfos.
        since we also need to wait on the rendering to finish before presenting we use a second semaphore (signalSemaphoreInfo + semaphores[imageIndex]) which is signalled when the commands past the OM that writed to the swapchain image has been completed on the gpu.
            we set it to be signalled at that specific stage by using the semaphore as an entry to pSignalSemaphoreInfos.
        then later during VkPresentInfoKHR we set the same semaphore as the semaphore that the present-engine needs to wait for before presenting.

        also, we use both frameIndex and imageIndex since the frameIndex might not match the actual imageIndex that is returned by vkAcquireNextImageKHR (since it might not give consecutive indices).
        */
        RenderAPI::CommandQueue::SubmitDesc submitDesc{
            .CommandBufferPtr = &cb,
            .OnCompletionFence = &fences[frameIndex],
            .WaitOnSemaphores = {{.SemaphorePtr = &swapchain.GetImageReadySemaphore(), .Stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT}},
            .SignalSemaphores = {{.SemaphorePtr = &swapchain.GetRenderCompletedSemaphore(), .Stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT}}
        };

        graphicsQueue.SubmitCommandBuffers(submitDesc);

        RenderAPI::Swapchain::PresentDesc presentDesc{
            .Queue = &graphicsQueue,
            .WaitOnSemaphores = { &swapchain.GetRenderCompletedSemaphore() }
        };
        swapchain.Present(presentDesc);

        frameIndex = (frameIndex + 1) % swapchain.GetFramesInFlight();

        // Poll events
    }

    core.FlushGPU();

    aZero::Window::Shutdown();

	return 0;
}