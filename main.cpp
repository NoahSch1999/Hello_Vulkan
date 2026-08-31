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
#include "Loading_Assets/Geo_PassStructs.hpp"


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

    meshletMeshData = aZero::File::ExtractGltfMeshletMesh(asset->meshes[1], asset);
    imageData = aZero::File::ExtractGltfImage(asset->images[1], asset, path);
    return true;
}

using namespace aZero;


struct RenderingVariables
{
    Compiler::ShaderProgram msProgram;
    Compiler::ShaderProgram csCullProgram;
    Pipeline::RenderPass msRenderPass;
    Pipeline::RenderPass csCullPass;

    RenderAPI::Buffer
        cameraBuffer,
        meshletBoundsBuffer,
        instanceDataBuffer,
        materialBuffer,
        iaBuffer,
        meshDataBuffer,
        instanceCullDataBuffer,
        drawMetadataBuffer,
        scratchBuffer
        ;
};


void SetupRenderingPasses(RenderAPI::Core& core, Compiler::Compiler& compiler, RenderingVariables& vars)
{
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

    Compiler::CompilerSession session = compiler.CreateSession(d).value();

    // CS
    Compiler::ShaderModule moduleComputeCull = session.LoadModule("C:/Coding/Hello_Vulkan/Shaders/IA_Pipeline/Geo_ComputeCull.slang").value();
    Compiler::ShaderProgram::ComputeDesc csProgramDesc{
        .EntryPointName = {"main"},
        .Modules = { &moduleComputeCull }
    };
    vars.csCullProgram.Build(csProgramDesc);

    vars.csCullPass.Init(core);
    Pipeline::RenderPass::ComputePassDesc csCullPassDesc{
        .Program = &vars.csCullProgram
    };
    vars.csCullPass.Compile(csCullPassDesc);

    // MS
    Compiler::ShaderModule moduleTaskShaderPass = session.LoadModule("C:/Coding/Hello_Vulkan/Shaders/IA_Pipeline/Geo_TaskShader.slang").value();
    Compiler::ShaderModule moduleMeshShaderPass = session.LoadModule("C:/Coding/Hello_Vulkan/Shaders/IA_Pipeline/Geo_MeshShader.slang").value();
    Compiler::ShaderModule modulePixelShaderPass = session.LoadModule("C:/Coding/Hello_Vulkan/Shaders/IA_Pipeline/Geo_PixelShader.slang").value();

    Compiler::ShaderProgram::MeshletDesc msProgramDesc{
        .EntryPoints = {"asMain", "msMain", "psMain"},
        .Modules = {&moduleTaskShaderPass, &moduleMeshShaderPass, &modulePixelShaderPass}
    };
    vars.msProgram.Build(msProgramDesc);

    vars.msRenderPass.Init(core);
    Pipeline::RenderPass::MeshShaderPassDesc mspDesc;
    mspDesc.Program = &vars.msProgram;
    mspDesc.DepthDesc = Pipeline::RenderPass::Desc::DepthStencil();
    mspDesc.RenderTargetFormats = { VK_FORMAT_B8G8R8A8_SRGB };
    mspDesc.RasterDesc.CullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE;
    vars.msRenderPass.Compile(mspDesc);

    // RESOURCES

    RenderAPI::Buffer::Desc passBufferDesc{
        .Size = 1024 * 1024,
        .UsageBits = RenderAPI::EBufferUsageBits::DeviceAddress,
        .MemoryType = RenderAPI::EMemoryType::BestFit,
        .SeperateHeap = false
    };

    passBufferDesc.Name = "cameraBuffer";
    vars.cameraBuffer = RenderAPI::Buffer(core, passBufferDesc);

    passBufferDesc.Name = "meshletBoundsBuffer";
    vars.meshletBoundsBuffer = RenderAPI::Buffer(core, passBufferDesc);

    passBufferDesc.Name = "instanceDataBuffer";
    vars.instanceDataBuffer = RenderAPI::Buffer(core, passBufferDesc);

    passBufferDesc.Name = "instanceCullDataBuffer";
    vars.instanceCullDataBuffer = RenderAPI::Buffer(core, passBufferDesc);

    passBufferDesc.Name = "drawMetadataBuffer";
    vars.drawMetadataBuffer = RenderAPI::Buffer(core, passBufferDesc);

    passBufferDesc.Name = "materialBuffer";
    vars.materialBuffer = RenderAPI::Buffer(core, passBufferDesc);

    passBufferDesc.Name = "meshDataBuffer";
    passBufferDesc.Size = meshletMeshData.Meshlets.size() * sizeof(meshletMeshData.Meshlets[0]) + meshletMeshData.Vertices.size() * sizeof(meshletMeshData.Vertices[0]);
    vars.meshDataBuffer = RenderAPI::Buffer(core, passBufferDesc);

    passBufferDesc.Name = "iaBuffer";
    passBufferDesc.UsageBits = RenderAPI::EBufferUsageBits::DeviceAddress | RenderAPI::EBufferUsageBits::Indirect | RenderAPI::EBufferUsageBits::TransferDst;
    passBufferDesc.Size = 1024 * 1024,
        vars.iaBuffer = RenderAPI::Buffer(core, passBufferDesc);

    passBufferDesc.Name = "scratchBuffer";
    passBufferDesc.UsageBits = RenderAPI::EBufferUsageBits::DeviceAddress | RenderAPI::EBufferUsageBits::TransferSrc;
    vars.scratchBuffer = RenderAPI::Buffer(core, passBufferDesc);

    aZero::Rendering::PBRMaterial material{
        .AlbedoTexture = 0
    };

    vars.scratchBuffer.Upload(0, 0);
    vars.materialBuffer.Upload(material, 0);
    vars.meshDataBuffer.Upload(meshletMeshData.Meshlets, 0);
    vars.meshDataBuffer.Upload(meshletMeshData.Vertices, meshletMeshData.Meshlets.size() * sizeof(meshletMeshData.Meshlets[0]));
    vars.meshletBoundsBuffer.Upload(meshletMeshData.MeshletBounds, 0);

    aZero::Rendering::InstanceData instanceData{
        .WorldMatrix = glm::rotate(glm::mat4(1.0), glm::radians(180.f), glm::vec3(0.f, 1.f, 0.f)),
        .MeshletBDA = vars.meshDataBuffer.GetDeviceAddress().value(),
        .VertexBDA = vars.meshDataBuffer.GetDeviceAddress().value() + (VkDeviceAddress)(meshletMeshData.Meshlets.size() * sizeof(meshletMeshData.Meshlets[0])),
        .MeshletCount = meshletMeshData.Submeshes[0].MeshletCount,
        .MaterialIndex = 0
    };

    aZero::Rendering::InstanceCullData instanceCullData{
        .WorldSpaceBounds = {
            .Origin = glm::vec3(0.f, 1.f, 0.f),
            .Radius = meshletMeshData.Submeshes[0].Bounds.Radius
        },
        .MeshletCount = meshletMeshData.Submeshes[0].MeshletCount
    };

    vars.instanceDataBuffer.Upload(instanceData, 0);
    vars.instanceCullDataBuffer.Upload(instanceCullData, 0);

    instanceData.MeshletCount = meshletMeshData.Submeshes[1].MeshletCount;
    instanceData.MeshletBDA = vars.meshDataBuffer.GetDeviceAddress().value() + (VkDeviceAddress)(meshletMeshData.Submeshes[0].MeshletCount * sizeof(meshletMeshData.Meshlets[0]));

    instanceCullData.MeshletCount = meshletMeshData.Submeshes[1].MeshletCount;
    instanceCullData.WorldSpaceBounds.Radius = meshletMeshData.Submeshes[1].Bounds.Radius;

    vars.instanceDataBuffer.Upload(instanceData, sizeof(instanceData));
    vars.instanceCullDataBuffer.Upload(instanceCullData, sizeof(instanceCullData));
}

int main()
{

    bool loaded = LoadAssets("C:/Coding/aZeroEditor/content/scenes/TEST2.gltf");
    if (!loaded) {
        std::cout << "Failed to load assets\n";
        exit(-1);
    }

    //Package::AssetPackage assetPac("C:/Coding/Hello_Vulkan/MyPac.pac");

    //imageData = assetPac.GetAsset<aZero::Asset::ImageData>(0);

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
    bool isinit = core.IsInitialized();
    std::cout << "Initialized: " << isinit << "\n";

    // ----------------------------------------------------------------------------------------------------
    // create and compile slang shader
    Compiler::Compiler compiler(core);

    // ----------------------------------------------------------------------------------------------------

    RenderAPI::CommandQueue graphicsQueue(core, RenderAPI::EQueueType::Graphics);

    // setup window and surface (thanks aZeroWindow <3)
    RenderWindow window(aZero::Window::WindowDesc("Hello Vulkan", aZero::Window::WindowDesc::Rect({ 0, 0, 1920, 1080 }), { 0,0,0,0 }, SDL_WINDOW_RESIZABLE, {}));

    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window.GetSDLWindow(), core.GetInstance(), nullptr, &surface);

    RenderAPI::Swapchain::Desc swapChainDesc{
        .Surface = surface,
        .Format = RenderAPI::EImageFormat::VK_FORMAT_B8G8R8A8_SRGB,
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

    RenderingVariables vars;
    SetupRenderingPasses(core, compiler, vars);

    while (window.IsOpen())
    {
        window.PollEvents();

        // Wait on fence
        // we use fences since we cant read semaphores on the cpu (its only for gpu-synchronization)
        fences[frameIndex].WaitFor();
        fences[frameIndex].Reset();

        swapchain.NextFrame(frameIndex);

        // Update shader data6
        if (GetAsyncKeyState('W'))
        {
            camPos.z += 0.001f;
        }
        if (GetAsyncKeyState('S'))
        {
            camPos.z -= 0.001f;
        }
        if (GetAsyncKeyState('A'))
        {
            camPos.x += 0.001f;
        }
        if (GetAsyncKeyState('D'))
        {
            camPos.x -= 0.001f;
        }
        if (GetAsyncKeyState(VK_SPACE))
        {
            camPos.y += 0.001f;
        }
        if (GetAsyncKeyState(VK_LCONTROL))
        {
            camPos.y -= 0.001f;
        }

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)window.GetClientDimensions()[0] / (float)window.GetClientDimensions()[1], 0.1f, 1000.0f);
        glm::mat4 view = glm::translate(glm::mat4(1.0f), camPos);

        aZero::Rendering::CameraData cameraData{
            .ViewMatrix = view,
            .ViewProjectionMatrix = projection * view
        };
        vars.cameraBuffer.Upload(cameraData, frameIndex * sizeof(aZero::Rendering::CameraData));

        // Record command buffer
        RenderAPI::CommandBuffer& cb = cmdBuffers[frameIndex];
        cb.Reset(); // reset (needs to be done after checking that its no longer in use)

        cb.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        {
            RenderAPI::Buffer::TransitionDesc iaTrCpyDesc{
                .DstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .Offset = 0,
                .Size = (uint32_t)sizeof(uint32_t)
            };
            vars.iaBuffer.Transition(iaTrCpyDesc);

            RenderAPI::Buffer::TransitionDesc sratchTrDesc{
                .DstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .DstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .Offset = 0,
                .Size = (uint32_t)sizeof(uint32_t)
            };
            vars.scratchBuffer.Transition(sratchTrDesc);

            cb.Barrier({}, {}, { vars.iaBuffer.GetTransition(),  vars.scratchBuffer.GetTransition() });
            cb.Copy(vars.scratchBuffer, 0, vars.iaBuffer, 0, sizeof(aZero::Rendering::IndirectArgumentCounter));

            RenderAPI::Buffer::TransitionDesc iaTrDesc{
                .DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .Offset = 0,
                .Size = (uint32_t)vars.iaBuffer.GetDesc().Size
            };
            vars.iaBuffer.Transition(iaTrDesc);
            
            RenderAPI::Buffer::TransitionDesc drawDataTrDesc{
                .DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .Offset = 0,
                .Size = (uint32_t)vars.drawMetadataBuffer.GetDesc().Size
            };
            vars.drawMetadataBuffer.Transition(drawDataTrDesc);

            cb.Barrier({}, {}, { vars.iaBuffer.GetTransition(), vars.drawMetadataBuffer.GetTransition() });
        }

        vars.csCullPass.Render(cb, {
            .GroupX = 1, .GroupY = 1, .GroupZ = 1,
            .DescriptorSets = {},
            .Callback = [&](RenderAPI::CommandBuffer& cb, Pipeline::RenderPass& pass) {
                 aZero::Rendering::InstanceCullPushConstants constants {
                        .IndirectArgumentBufferPtr = vars.iaBuffer.GetDeviceAddress().value(),
                         .CameraBufferPtr = vars.cameraBuffer.GetDeviceAddress().value() + frameIndex * sizeof(aZero::Rendering::CameraData),
                         .InstanceCullDataBufferPtr = vars.instanceCullDataBuffer.GetDeviceAddress().value(),
                         .DrawMetadataBufferPtr = vars.drawMetadataBuffer.GetDeviceAddress().value(),
                         .InstanceCount = 2
                    };

                 cb.SetPushConstants(pass.GetPushConstantBlock("Constants"), constants);
            }
        });

        {
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

            RenderAPI::Buffer::TransitionDesc iaDesc{
                .DstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .DstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .Offset = 0,
                .Size = (uint32_t)vars.iaBuffer.GetDesc().Size
            };
            vars.iaBuffer.Transition(iaDesc);

            RenderAPI::Buffer::TransitionDesc drawDataTrDesc{
                .DstStageMask = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_NV,
                .DstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .Offset = 0,
                .Size = (uint32_t)vars.drawMetadataBuffer.GetDesc().Size
            };
            vars.drawMetadataBuffer.Transition(drawDataTrDesc);

            cb.Barrier({}, { outputBarriers[0], outputBarriers[1] }, { vars.iaBuffer.GetTransition() });
        }

        vars.msRenderPass.Render(
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
                        .x = 0.f,
                        .y = 0.f,
                        .width = static_cast<float>(window.GetClientDimensions()[0]),
                        .height = static_cast<float>(window.GetClientDimensions()[1]),
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f
                    };
                    VkRect2D scissor{.extent{.width = static_cast<uint32_t>(window.GetClientDimensions()[0]), .height = static_cast<uint32_t>(window.GetClientDimensions()[1]) } };
                    cb.SetScizzorRectsAndViewports({ vp }, { scissor });

                    aZero::Rendering::GeoDrawPushConstants constants{
                         .CameraBufferPtr = vars.cameraBuffer.GetDeviceAddress().value() + frameIndex * sizeof(aZero::Rendering::CameraData),
                         .MeshletBoundBufferPtr = vars.meshletBoundsBuffer.GetDeviceAddress().value(),
                         .MaterialBufferPtr = vars.materialBuffer.GetDeviceAddress().value(),
                         .InstanceDataBufferPtr = vars.instanceDataBuffer.GetDeviceAddress().value(),
                         .DrawMetadataBufferPtr = vars.drawMetadataBuffer.GetDeviceAddress().value(),
                         .RenderMode = 0u
                    };
                    cb.SetPushConstants(pass.GetPushConstantBlock("Constants"), constants);

                    RenderAPI::CommandBuffer::ExecuteIndirectDesc exeDesc{
                        .IndirectArgumentBuffer = &vars.iaBuffer,
                        .IndirectArgumentBufferOffset = (uint32_t)(sizeof(aZero::Rendering::IndirectArgumentCounter)), // Starts after count
                        .IndirectArgumentStride = (uint32_t)(sizeof(uint32_t) * 3),
                        .CountBuffer = &vars.iaBuffer,
                        .CountBufferOffset = 0,
                        .MaxDraws = 2
                    };
                    cb.ExecuteIndirectMeshTasksCount(exeDesc);
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