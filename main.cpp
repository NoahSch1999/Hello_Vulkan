#include <iostream>
#include <vector>

#include "slang.h"
#include "slang/slang-com-ptr.h"

#include <SDL3/SDL_vulkan.h>

#include "aZeroWindow.hpp"
#include "Loading_Assets/Gltf.hpp"
#include "Loading_Assets/ImageLoading.hpp"

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
    imageData = aZero::File::ExtractGltfImage(asset->images[0], asset, path);
}

int main()
{
    using namespace aZero;

    bool loaded = LoadAssets("C:/Projects/Programming/Hello_Vulkan/KtxCube.gltf");
    if (!loaded) {
        std::cout << "Failed to load assets\n";
        exit(-1);
    }

    aZero::Window::Init(SDL_INIT_VIDEO);

    SDL_Vulkan_LoadLibrary(NULL);

    RenderAPI::Core::Desc coreDesc {
        .ApplicationName = "Hello Vulkan",
        .EnableValidationLayer = true
    };
    RenderAPI::Core core(coreDesc);
    RenderAPI::CommandQueue graphicsQueue(core, RenderAPI::EQueueType::Graphics);

    // setup window and surface (thanks aZeroWindow <3)
    RenderWindow window(aZero::Window::WindowDesc("Hello Vulkan", aZero::Window::WindowDesc::Rect({ 0, 0, 1920, 1080 }), { 0,0,0,0 }, SDL_WINDOW_RESIZABLE, {}));

    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window.GetSDLWindow(), core.GetInstance(), nullptr, &surface);

    RenderAPI::Swapchain::Desc swapChainDesc{
        .Surface = surface,
        .Format = VK_FORMAT_B8G8R8A8_SRGB,
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
        .AccessUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .MemoryUsage = VMA_MEMORY_USAGE_AUTO
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
        .AccessUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .MemoryUsage = VMA_MEMORY_USAGE_AUTO
    };

    for (auto i = 0; i < shaderDataBuffers.size(); i++) {
        shaderDataBuffers[i] = RenderAPI::Buffer(core, shaderDataBufferDesc);
    }

    std::vector<RenderAPI::Fence> fences(swapchain.GetFramesInFlight());

    for (auto i = 0; i < swapchain.GetFramesInFlight(); i++) {
        fences[i] = RenderAPI::Fence(core);
    }

    RenderAPI::CommandPool::Desc cmdPoolDesc{
        .QueueType = RenderAPI::EQueueType::Graphics
    };
    RenderAPI::CommandPool cmdPool(core, cmdPoolDesc);

    std::vector<RenderAPI::CommandBuffer> cmdBuffers = cmdPool.CreateCommandBuffers(core, swapchain.GetFramesInFlight());

    RenderAPI::Image::Desc meshImageDesc{
        .Format = imageData.Format,
        .Type = imageData.Type,
        .Usage = RenderAPI::EImageUsageFlags::TransferDst | RenderAPI::EImageUsageFlags::Sampled,
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
       .AccessUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
       .Flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
       .MemoryUsage = VMA_MEMORY_USAGE_AUTO
    };

    RenderAPI::Buffer imgSrcBuffer(core, imgSrcBufferDesc);

    imgSrcBuffer.Upload(imageData.Data.data(), imageData.Data.size(), 0);

    std::vector<uint32_t> mipOffsets;
    for (auto& ele : imageData.MipLevels) {
        mipOffsets.push_back(ele.Offset);
    }

    RenderAPI::Image::UploadDesc meshImageUploadDesc{
       .BaseWidth = imageData.BaseWidth,
       .BaseHeight = imageData.BaseHeight,
       .DepthOrArrayCount = imageData.DepthOrArrayCount,
       .MipOffsets = mipOffsets
    };
    meshImage.Upload(core, graphicsQueue, cmdPool, imgSrcBuffer, meshImageUploadDesc);

    RenderAPI::Sampler sampler(core, {});

    RenderAPI::DescriptorSet::Desc setDesc{ .Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .DescriptorCount = 100, .ShaderStages = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT };
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

    // create and compile slang shader
    Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;
    slang::createGlobalSession(slangGlobalSession.writeRef());

    auto slangTargets{ std::to_array<slang::TargetDesc>({ {
    .format{SLANG_SPIRV},
    .profile{slangGlobalSession->findProfile("spirv_1_4")}} }) };

    auto slangOptions = std::to_array<slang::CompilerOptionEntry>({
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

    slang::SessionDesc slangSessionDesc{
        .targets{slangTargets.data()},
        .targetCount{SlangInt(slangTargets.size())},
        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
        .compilerOptionEntries{slangOptions.data()},
        .compilerOptionEntryCount{uint32_t(slangOptions.size())}
    };
    Slang::ComPtr<slang::ISession> slangSession;
    slangGlobalSession->createSession(slangSessionDesc, slangSession.writeRef());

    Slang::ComPtr<slang::IModule> slangModule{
    slangSession->loadModuleFromSource("triangle", "C:/Projects/Programming/Hello_Vulkan/Loading_Assets/shader.slang", nullptr, nullptr)
    };
    Slang::ComPtr<ISlangBlob> spirv;
    slangModule->getTargetCode(0, spirv.writeRef());


    VkShaderModuleCreateInfo shaderModuleCI{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = spirv->getBufferSize(),
    .pCode = (uint32_t*)spirv->getBufferPointer()
    };
    VkShaderModule shaderModule{};
    aZero::RenderAPI::chkSucceed(vkCreateShaderModule(core.GetLogicalDevice(), &shaderModuleCI, nullptr, &shaderModule));

    // create pipeline
    VkPushConstantRange pushConstantRange{
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    .size = sizeof(VkDeviceAddress)
    };

    std::vector<VkDescriptorSetLayout> descLayouts{ descriptorSet.GetLayout() };
    VkPipelineLayoutCreateInfo pipelineLayoutCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = (uint32_t)descLayouts.size(),
        .pSetLayouts = descLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkPipelineLayout pipelineLayout;
    aZero::RenderAPI::chkSucceed(vkCreatePipelineLayout(core.GetLogicalDevice(), &pipelineLayoutCI, nullptr, &pipelineLayout));

    VkVertexInputBindingDescription vertexBinding{
     .binding = 0,
     .stride = sizeof(aZero::Asset::Vertex),
     .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    std::vector<VkVertexInputAttributeDescription> vertexAttributes{ // input layout
    {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT },
    {.location = 1, .binding = 0, .format = VK_FORMAT_R32_UINT, .offset = sizeof(glm::vec3) },
    {.location = 2, .binding = 0, .format = VK_FORMAT_R32_UINT, .offset = sizeof(glm::vec3) + sizeof(uint32_t)},
    };

    VkPipelineVertexInputStateCreateInfo vertexInputState{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &vertexBinding,
    .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
    .pVertexAttributeDescriptions = vertexAttributes.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{ // defines how the primitives are assembled
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages{ // sets shaders. extracted from the slang shader module
    {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = shaderModule, .pName = "main"},
    {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = shaderModule, .pName = "main" }
    };

    VkPipelineViewportStateCreateInfo viewportState{ // specify viewport and scizzorrect
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .scissorCount = 1
    };
    std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR }; // make them dynamic so we can change them without recompiling the pipelinestate
    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates.data()
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilState{ // depthstencil settings
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
    };

    std::vector<VkFormat> imageFormats{ VK_FORMAT_B8G8R8A8_SRGB };
    VkPipelineRenderingCreateInfo renderingCI{ // this tells the rendering pipeline that we are using "dynamic rendering"
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .colorAttachmentCount = (uint32_t)imageFormats.size(),
    .pColorAttachmentFormats = imageFormats.data(),
    .depthAttachmentFormat = RenderAPI::ToVK_ImageFormat(depthImage.Get().GetDesc().Format)
    };

    VkPipelineColorBlendAttachmentState blendAttachment{ // blend desc
    .colorWriteMask = 0xF
    };
    VkPipelineColorBlendStateCreateInfo colorBlendState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &blendAttachment
    };
    VkPipelineRasterizationStateCreateInfo rasterizationState{ // raster desc
         .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
         .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampleState{ // sample desc
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkGraphicsPipelineCreateInfo pipelineCI{
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext = &renderingCI, // pass the VkPipelineRenderingCreateInfo as pNext since theres no dedicated member in the VkGraphicsPipelineCreateInfo struct (since dynamic rendering was added later)
    .stageCount = 2,
    .pStages = shaderStages.data(),
    .pVertexInputState = &vertexInputState,
    .pInputAssemblyState = &inputAssemblyState,
    .pViewportState = &viewportState,
    .pRasterizationState = &rasterizationState,
    .pMultisampleState = &multisampleState,
    .pDepthStencilState = &depthStencilState,
    .pColorBlendState = &colorBlendState,
    .pDynamicState = &dynamicState,
    .layout = pipelineLayout
    };
    VkPipeline pipeline;

    // VkPipelineCache can be used here to cache the result for future re-use. This can be saved and then loaded again between app runs.
    aZero::RenderAPI::chkSucceed(vkCreateGraphicsPipelines(core.GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

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

        cb.Barrier({ outputBarriers[0], outputBarriers[1] });
        
        // DYNAMIC RENDERING
        VkRenderingAttachmentInfo colorAttachmentInfo{ // specific to dynamic rendering
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchain.GetCurrentBackBufferView(),
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue{.color{ 0.0f, 0.0f, 0.2f, 1.0f }}
        };
        VkRenderingAttachmentInfo depthAttachmentInfo{ // specific to dynamic rendering
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depthImageView.Get(),
            .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = {.depthStencil = {1.0f,  0}}
        };

        VkRenderingInfo renderingInfo{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea{.extent{.width = static_cast<uint32_t>(window.GetClientDimensions()[0]), .height = static_cast<uint32_t>(window.GetClientDimensions()[1]) }},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
            .pDepthAttachment = &depthAttachmentInfo
        };
        cb.BeginRender(renderingInfo);
        //

        // since we made them dynamic when creating the pipeline we overwrite them here
        VkViewport vp{
            .width = static_cast<float>(window.GetClientDimensions()[0]),
            .height = static_cast<float>(window.GetClientDimensions()[1]),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        VkRect2D scissor{ .extent{.width = static_cast<uint32_t>(window.GetClientDimensions()[0]), .height = static_cast<uint32_t>(window.GetClientDimensions()[1]) } };
        cb.SetScizzorRectsAndViewports({ vp }, { scissor });

        vkCmdBindPipeline(cb.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        std::vector<VkDescriptorSet> descSets{ descriptorSet.Get() };
        vkCmdBindDescriptorSets(cb.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descSets.size(), descSets.data(), 0, nullptr);
        
        cb.SetVertexBuffer(vBuffer, 0);
        cb.SetIndexBuffer(vBuffer, sizeof(aZero::Asset::Vertex) * meshData.Vertices.size());
       
        auto pushconst = shaderDataBuffers[frameIndex].GetDeviceAddress().value();
        vkCmdPushConstants(cb.Get(), pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &pushconst);

        cb.DrawIndexed((uint32_t)meshData.Indices.size(), 3, 0, 0, 0);

        cb.EndRender();

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

    aZero::Window::Shutdown();

	return 0;
}