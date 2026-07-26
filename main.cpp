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
#include "render_api/DescriptorPool.hpp"

class RenderWindow : public aZero::Window::Window_Win32 {
public:
    RenderWindow() = default;
    explicit RenderWindow(const aZero::Window::WindowDesc& desc)
        :Window_Win32(desc)
    {}
private:
    virtual void PollEventImpl(const SDL_Event& event) override {
        if (GetAsyncKeyState(VK_ESCAPE)) {
            this->Close();
        }
    }
};

static inline void chk(VkResult result) {
    if (result != VK_SUCCESS) {
        std::cerr << "Vulkan call returned an error (" << result << ")\n";
        exit(result);
    }
}

static inline void chk(bool result) {
    if (!result) {
        std::cerr << "Call returned an error\n";
        exit(result);
    }
}

bool updateSwapchain = false;
static inline void chkSwapchain(VkResult result) {
    if (result < VK_SUCCESS) {
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            updateSwapchain = true;
            return;
        }
        std::cerr << "Vulkan call returned an error (" << result << ")\n";
        exit(result);
    }
}

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

/*
VkInstance - Device, initializes the vulkan lib

*/

/*
Q:
- what is VkAllocationCallbacks

 chk(vkCreateImage(device, &depthImageCI, nullptr, &depthImage));
    VkDeviceMemory vkmem;
    VkMemoryAllocateInfo vkAllocInfo;
    vkAllocateMemory(device, &vkAllocInfo, nullptr, &vkmem);
*/

void UploadDataToImage(const aZero::Asset::ImageData& data, VkImage image, VkBuffer stagingBuffer, VkQueue queue, VkDevice device, VkCommandPool commandPool)
{
    VkFenceCreateInfo fenceOneTimeCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fenceOneTime{};
    chk(vkCreateFence(device, &fenceOneTimeCI, nullptr, &fenceOneTime));
    VkCommandBuffer cbOneTime{};
    VkCommandBufferAllocateInfo cbOneTimeAI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = commandPool, .commandBufferCount = 1 };
    chk(vkAllocateCommandBuffers(device, &cbOneTimeAI, &cbOneTime));

    VkCommandBufferBeginInfo cbOneTimeBI{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    chk(vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI));
    VkImageMemoryBarrier2 barrierTexImage{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = (uint32_t)data.MipLevels.size(), .layerCount = 1}
    };
    VkDependencyInfo barrierTexInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrierTexImage
    };
    vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
    std::vector<VkBufferImageCopy> copyRegions{};
    for (auto j = 0; j < data.MipLevels.size(); j++) {
        copyRegions.push_back({
            .bufferOffset = data.MipLevels[j].Offset,
            .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = (uint32_t)j, .layerCount = 1},
            .imageExtent{.width = data.BaseWidth >> j, .height = data.BaseHeight >> j, .depth = data.DepthOrArrayCount },
            });
    }
    vkCmdCopyBufferToImage(cbOneTime, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());
    VkImageMemoryBarrier2 barrierTexRead{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .image = image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = (uint32_t)data.MipLevels.size(), .layerCount = 1 }
    };
    barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
    vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
    chk(vkEndCommandBuffer(cbOneTime));
    VkCommandBufferSubmitInfo cbOneTimeSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cbOneTime
    };
    VkSubmitInfo2 oneTimeSI{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cbOneTimeSubmitInfo
    };
    chk(vkQueueSubmit2(queue, 1, &oneTimeSI, fenceOneTime));
    chk(vkWaitForFences(device, 1, &fenceOneTime, VK_TRUE, UINT64_MAX));
}

int main()
{
    using namespace aZero;

    bool loaded = LoadAssets("C:/Projects/Programming/Hello_Vulkan/KtxCube.gltf");
    if (!loaded) {
        std::cout << "Failed to load assets\n";
        exit(-1);
    }

    chk(SDL_Init(SDL_INIT_VIDEO));
    chk(SDL_Vulkan_LoadLibrary(NULL));

    RenderAPI::Core::Desc coreDesc {
        .ApplicationName = "Hello Vulkan",
        .EnableValidationLayer = true
    };
    RenderAPI::Core core(coreDesc);

    // setup window and surface (thanks aZeroWindow <3)
    RenderWindow window(aZero::Window::WindowDesc("Hello Vulkan", aZero::Window::WindowDesc::Rect({ 0, 0, 1920, 1080 }), { 0,0,0,0 }, SDL_WINDOW_RESIZABLE, {}));

    VkSurfaceKHR surface;
    chk(SDL_Vulkan_CreateSurface(window.GetSDLWindow(), core.GetInstance(), nullptr, &surface));

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

    RenderAPI::ImageView depthImageView(core, depthImageViewDesc);

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

    VkSemaphoreCreateInfo semaphoreCI{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceCI{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    std::vector<VkSemaphore> semaphores(swapchain.GetFramesInFlight());
    std::vector<VkSemaphore> swapchainSemaphores(swapchain.GetFramesInFlight());
    std::vector<VkFence> fences(swapchain.GetFramesInFlight());

    for (auto i = 0; i < swapchain.GetFramesInFlight(); i++) {
        semaphores[i] = core.CreateSemaphoreVk();
        swapchainSemaphores[i] = core.CreateSemaphoreVk();
        fences[i] = core.CreateFence();
    }

    RenderAPI::CommandPool::Desc cmdPoolDesc{
        .QueueType = RenderAPI::EQueueType::Graphics
    };
    RenderAPI::CommandPool cmdPool(core, cmdPoolDesc);

    std::vector<VkCommandBuffer> cmdBuffers = cmdPool.CreateCommandBuffers(core, swapchain.GetFramesInFlight());

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
    meshImage.Upload(core, cmdPool, imgSrcBuffer, meshImageUploadDesc);

    RenderAPI::Sampler sampler(core, {});

    RenderAPI::DescriptorPool::Desc descriptorPoolDesc{
        .Count = 1000
    };
    RenderAPI::DescriptorPool descriptorPool(core, descriptorPoolDesc);
    RenderAPI::DescriptorHandle descriptorHandle{
        .Sampler = &sampler,
        .View = &meshImageView,
        .Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        .Index = 0
    };
    descriptorPool.UploadDescriptor(core, descriptorHandle);

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
    chk(vkCreateShaderModule(core.GetLogicalDevice(), &shaderModuleCI, nullptr, &shaderModule));

    // create pipeline
    VkPushConstantRange pushConstantRange{
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    .size = sizeof(VkDeviceAddress)
    };

    std::vector<VkDescriptorSetLayout> descLayouts{ descriptorPool.GetLayout() };
    VkPipelineLayoutCreateInfo pipelineLayoutCI{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = (uint32_t)descLayouts.size(),
        .pSetLayouts = descLayouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkPipelineLayout pipelineLayout;
    chk(vkCreatePipelineLayout(core.GetLogicalDevice(), &pipelineLayoutCI, nullptr, &pipelineLayout));

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
    chk(vkCreateGraphicsPipelines(core.GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

    uint64_t lastTime{ SDL_GetTicks() };
    uint32_t frameIndex = 0;
    uint32_t imageIndex{ 0 };

    glm::vec3 camPos{ 0.0f, 0.0f, -6.0f };
    glm::vec3 objectRotations[3]{};

    while (window.IsOpen())
    {
        window.PollEvents();

        // Wait on fence
        // we use fences since we cant read semaphores on the cpu (its only for gpu-synchronization)
        chk(vkWaitForFences(core.GetLogicalDevice(), 1, &fences[frameIndex], true, UINT64_MAX));
        chk(vkResetFences(core.GetLogicalDevice(), 1, &fences[frameIndex]));
        // Acquire next image
        chkSwapchain(vkAcquireNextImageKHR(core.GetLogicalDevice(), swapchain.Get(), UINT64_MAX, swapchainSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex)); // might not give us imageIndex that are in consecutive order, so we have a seperate index for the current swapchain and frameindex

        // Update shader data
        shaderData.projection = glm::perspective(glm::radians(45.0f), (float)window.GetClientDimensions()[0] / (float)window.GetClientDimensions()[1], 0.1f, 32.0f);
        shaderData.view = glm::translate(glm::mat4(1.0f), camPos);
        for (auto i = 0; i < 3; i++) {
            auto instancePos = glm::vec3((float)(i - 1) * 3.0f, 0.0f, 0.0f);
            shaderData.model[i] = glm::translate(glm::mat4(1.0f), instancePos) * glm::mat4_cast(glm::quat(objectRotations[i]));
        }
        shaderDataBuffers[frameIndex].Upload(&shaderData, sizeof(ShaderData), 0);

        // Record command buffer
        auto cb = cmdBuffers[frameIndex];
        chk(vkResetCommandBuffer(cb, 0)); // reset (needs to be done after checking that its no longer in use)

        VkCommandBufferBeginInfo cbBI{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        chk(vkBeginCommandBuffer(cb, &cbBI)); // begin recording for real

        std::array<VkImageMemoryBarrier2, 2> outputBarriers{
            VkImageMemoryBarrier2{
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

                .image = swapchain.GetBackBuffer(imageIndex),
                .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
            },
            VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                .image = depthImage.Get().Get(),
                .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1 }
            }
        };
        VkDependencyInfo barrierDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 2,
            .pImageMemoryBarriers = outputBarriers.data()
        };
        vkCmdPipelineBarrier2(cb, &barrierDependencyInfo);
        
        // DYNAMIC RENDERING
        VkRenderingAttachmentInfo colorAttachmentInfo{ // specific to dynamic rendering
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchain.GetBackBufferView(imageIndex),
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
        vkCmdBeginRendering(cb, &renderingInfo);
        //

        // since we made them dynamic when creating the pipeline we overwrite them here
        VkViewport vp{
            .width = static_cast<float>(window.GetClientDimensions()[0]),
            .height = static_cast<float>(window.GetClientDimensions()[1]),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(cb, 0, 1, &vp);
        VkRect2D scissor{ .extent{.width = static_cast<uint32_t>(window.GetClientDimensions()[0]), .height = static_cast<uint32_t>(window.GetClientDimensions()[1]) } };
        vkCmdSetScissor(cb, 0, 1, &scissor);

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        VkDeviceSize vOffset{ 0 };
        std::vector<VkDescriptorSet> descSets{ descriptorPool.GetSet() };
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descSets.size(), descSets.data(), 0, nullptr);
        
        std::vector<VkBuffer> vBuffers{ vBuffer.Get() };
        vkCmdBindVertexBuffers(cb, 0, 1, vBuffers.data(), &vOffset);
        vkCmdBindIndexBuffer(cb, vBuffer.Get(), sizeof(aZero::Asset::Vertex) * meshData.Vertices.size(), VK_INDEX_TYPE_UINT32);

        auto pushconst = shaderDataBuffers[frameIndex].GetDeviceAddress().value();
        vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &pushconst);

        vkCmdDrawIndexed(cb, meshData.Indices.size(), 3, 0, 0, 0);

        vkCmdEndRendering(cb); // finish render pass

        VkImageMemoryBarrier2 barrierPresent{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = swapchain.GetBackBuffer(imageIndex),
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
        };
        VkDependencyInfo barrierPresentDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrierPresent
        };
        vkCmdPipelineBarrier2(cb, &barrierPresentDependencyInfo);

        vkEndCommandBuffer(cb); // end recording

        /*
        before we start outputting to the swapchain image we need to guarantee that the current swapchain image isnt in use by the present-engine.
        this is done by waiting on a semaphore (waitSemaphoreInfo + swapchainSemaphores[frameIndex]) that is signalled when vkAcquireNextImageKHR knows that the image isnt in use by the present-engine anymore.
            we wait my using the VkSemaphoreSubmitInfo as an entry into pWaitSemaphoreInfos.
        since we also need to wait on the rendering to finish before presenting we use a second semaphore (signalSemaphoreInfo + semaphores[imageIndex]) which is signalled when the commands past the OM that writed to the swapchain image has been completed on the gpu.
            we set it to be signalled at that specific stage by using the semaphore as an entry to pSignalSemaphoreInfos.
        then later during VkPresentInfoKHR we set the same semaphore as the semaphore that the present-engine needs to wait for before presenting.

        also, we use both frameIndex and imageIndex since the frameIndex might not match the actual imageIndex that is returned by vkAcquireNextImageKHR (since it might not give consecutive indices).
        */

        // Submit command buffer
        VkSemaphoreSubmitInfo waitSemaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = swapchainSemaphores[frameIndex], // this ensured that we wait to start the stageMask stage until this semaphore has been signalled (since its used as pWaitSemaphoreInfos) before executing further commands in the buffer
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, // this makes the wait happen on the color attachment stage, so prior work can still be done
        };
        VkSemaphoreSubmitInfo signalSemaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = semaphores[imageIndex], // this is signalled when the command buffer has passed the stageMask point (since its used as pSignalSemaphoreInfos)
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        VkCommandBufferSubmitInfo commandBufferSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cb
        };
        VkSubmitInfo2 submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSemaphoreInfo, // ensured that the command buffer isnt executed until the swapchain image has been released to the presentation engine and is free to be presented
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &commandBufferSubmitInfo,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalSemaphoreInfo, // this ensures that presentation of the swapchain image does not begin until the command buffer has finished execution
        };

        // we signal the fence since we cant read semaphores on the cpu (its only for gpu-synchronization)
        chk(vkQueueSubmit2(core.GetGraphicsQueue(), 1, &submitInfo, fences[frameIndex]));

        frameIndex = (frameIndex + 1) % swapchain.GetFramesInFlight();

        std::vector<VkSwapchainKHR> swapchains{ swapchain.Get() };
        // Present image
        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &semaphores[imageIndex], // here we wait on the semaphore that is signalled when the command buffer has passed the OM
            .swapchainCount = (uint32_t)swapchains.size(),
            .pSwapchains = swapchains.data(),
            .pImageIndices = &imageIndex
        };
        chkSwapchain(vkQueuePresentKHR(core.GetGraphicsQueue(), &presentInfo));

        // Poll events
    }

	return 0;
}