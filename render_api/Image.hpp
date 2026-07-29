#pragma once
#include "CommandPool.hpp"
#include "Buffer.hpp"
#include "CommandQueue.hpp"

namespace aZero::RenderAPI
{
    enum class EImageFormat {
        RGBA8_UNORM,
        RGBA8_UNORM_SRGB,
        BC7_UNORM,
        BC7_UNORM_SRGB,
        RGB32_FLOAT,
        RGB32_UINT,
        RGB32_SINT,
        RGBA32_FLOAT,
        RGBA32_UINT,
        RGBA32_SINT,
        RGBA16_FLOAT,
        RGBA16_UINT,
        RGBA16_SINT,
        RGBA16_UNORM,
        D24S8_UINT,
        UNKNOWN
    };

    enum class EImageType {
        Image1D,
        Image1D_Array,
        Image2D,
        Image2D_Array,
        ImageCube,
        ImageCube_Array,
        Image3D
    };

    enum EImageUsageFlags {
        TransferSrc = 0x00000001,
        TransferDst = 0x00000002,
        Sampled = 0x00000004,
        Storage = 0x00000008,
        Color_Attachment = 0x00000010,
        DepthStencil_Attachment = 0x00000020
    };

    enum EImageAspect {
        Color = 0x00000001,
        Depth = 0x00000002,
        Stencil = 0x00000004,
        DepthStencil = Depth | Stencil
    };

    // IF VULKAN!
    inline VkFormat ToVK_ImageFormat(EImageFormat format)
    {
        switch (format)
        {
        case EImageFormat::RGBA8_UNORM:
            return VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
        case EImageFormat::RGBA8_UNORM_SRGB:
            return VkFormat::VK_FORMAT_R8G8B8A8_SRGB;
        case EImageFormat::BC7_UNORM:
            return VkFormat::VK_FORMAT_BC7_UNORM_BLOCK;
        case EImageFormat::BC7_UNORM_SRGB:
            return VkFormat::VK_FORMAT_BC7_SRGB_BLOCK;
        case EImageFormat::D24S8_UINT:
            return VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
        default:
            return VkFormat::VK_FORMAT_UNDEFINED;
        }
    }

    inline VkImageType ToVK_ImageType(EImageType type)
    {
        switch (type)
        {
        case EImageType::Image1D:
            return VkImageType::VK_IMAGE_TYPE_1D;
        case EImageType::Image2D:
            return VkImageType::VK_IMAGE_TYPE_2D;
        case EImageType::Image3D:
            return VkImageType::VK_IMAGE_TYPE_3D;
        case EImageType::Image1D_Array:
            return VkImageType::VK_IMAGE_TYPE_1D;
        case EImageType::Image2D_Array:
            return VkImageType::VK_IMAGE_TYPE_2D;
        /* WHAT ABOUT CUBE MAPS? */
        default:
            return VkImageType::VK_IMAGE_TYPE_MAX_ENUM;
        }
    }

    inline VkImageViewType ToVK_ImageViewType(EImageType type)
    {
        switch (type)
        {
        case EImageType::Image1D:
            return VkImageViewType::VK_IMAGE_VIEW_TYPE_1D;
        case EImageType::Image2D:
            return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
        case EImageType::Image3D:
            return VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
        case EImageType::ImageCube:
            return VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE;

        case EImageType::Image1D_Array:
            return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case EImageType::Image2D_Array:
            return VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case EImageType::ImageCube_Array:
            return VkImageViewType::VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        default:
            return VkImageViewType::VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }
    }

    class Image
    {
    public:
        struct Desc
        {
            EImageFormat Format;
            EImageType Type;
            int32_t Usage;
            uint32_t Width = 0u, Height = 0u, Depth = 0u;
            uint32_t MipCount = 1u;
            uint32_t ArrayLayers = 1u;
        };

        struct UploadDesc
        {
            uint32_t BaseWidth, BaseHeight, DepthOrArrayCount;
            std::vector<uint32_t> MipOffsets;
        };


        Image() = default;
        Image(const Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        void Init(const Core& core, const Desc& desc)
        {
            // create depth image
            VkImageCreateInfo depthImageCI{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = ToVK_ImageType(desc.Type),
                .format = ToVK_ImageFormat(desc.Format),
                .extent = {.width = desc.Width, .height = desc.Height, .depth = desc.Depth },
                .mipLevels = desc.MipCount,
                .arrayLayers = desc.ArrayLayers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = (VkImageUsageFlags)desc.Usage,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };

            VmaAllocationCreateInfo allocCI{
                .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO
            };

            chkSucceed(vmaCreateImage(core.GetVmaAllocator(), &depthImageCI, &allocCI, &m_Image, &m_Allocation, nullptr));

            m_LastBarrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask = VK_ACCESS_2_NONE,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .image = m_Image,
                .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_NONE, .levelCount = 1, .layerCount = 1 }
            };

            m_Desc = desc;
        }

        struct TransitionDesc
        {
            VkPipelineStageFlags2 DstStageMask;
            VkAccessFlags2 DstAccessMask;
            VkImageLayout NewLayout;
            VkImageSubresourceRange SubresourceRange;
        };

        void Transition(const TransitionDesc& desc)
        {
            m_LastBarrier.srcStageMask = m_LastBarrier.dstStageMask;
            m_LastBarrier.srcAccessMask = m_LastBarrier.dstAccessMask;
            m_LastBarrier.oldLayout = m_LastBarrier.newLayout;

            m_LastBarrier.dstStageMask = desc.DstStageMask;
            m_LastBarrier.dstAccessMask = desc.DstAccessMask;
            m_LastBarrier.newLayout = desc.NewLayout;
            m_LastBarrier.subresourceRange = desc.SubresourceRange;
        }

        void Upload(const Core& core, CommandQueue& commandQueue, const CommandPool& commandPool, const Buffer& stagingBuffer, const UploadDesc& uploadDesc)
        {
            VkFenceCreateInfo fenceOneTimeCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            VkFence fenceOneTime{};
            chkSucceed(vkCreateFence(core.GetLogicalDevice(), &fenceOneTimeCI, nullptr, &fenceOneTime));
            VkCommandBuffer cbOneTime{};
            VkCommandBufferAllocateInfo cbOneTimeAI{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = commandPool.Get(), .commandBufferCount = 1};
            chkSucceed(vkAllocateCommandBuffers(core.GetLogicalDevice(), &cbOneTimeAI, &cbOneTime));

            VkCommandBufferBeginInfo cbOneTimeBI{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            };
            chkSucceed(vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI));

            this->Transition({
                .DstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .SubresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = (uint32_t)uploadDesc.MipOffsets.size(), .layerCount = 1}
            });

            VkDependencyInfo barrierTexInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = 1,
                .pImageMemoryBarriers = &m_LastBarrier
            };
            vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
            std::vector<VkBufferImageCopy> copyRegions{};
            for (uint32_t j = 0; j < uploadDesc.MipOffsets.size(); j++) {
                copyRegions.push_back({
                    .bufferOffset = uploadDesc.MipOffsets[j],
                    .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = j, .layerCount = 1},
                    .imageExtent{.width = uploadDesc.BaseWidth >> j, .height = uploadDesc.BaseHeight >> j, .depth = uploadDesc.DepthOrArrayCount },
                    });
            }
            vkCmdCopyBufferToImage(cbOneTime, stagingBuffer.Get(), m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

            this->Transition({
                .DstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .NewLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                .SubresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = (uint32_t)uploadDesc.MipOffsets.size(), .layerCount = 1}
            });
            barrierTexInfo.pImageMemoryBarriers = &m_LastBarrier;
            vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
            chkSucceed(vkEndCommandBuffer(cbOneTime));

            VkCommandBufferSubmitInfo cbOneTimeSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cbOneTime
            };
            VkSubmitInfo2 oneTimeSI{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &cbOneTimeSubmitInfo
            };
            chkSucceed(vkQueueSubmit2(commandQueue.Get(), 1, &oneTimeSI, fenceOneTime));
            chkSucceed(vkWaitForFences(core.GetLogicalDevice(), 1, &fenceOneTime, VK_TRUE, UINT64_MAX));
        }

        VkImage Get() const { return m_Image; }
        const Desc& GetDesc() const { return m_Desc; }
        const VkImageMemoryBarrier2& GetTransition() const { return m_LastBarrier; }
        void OverrideInternalMemoryBarrier(const VkImageMemoryBarrier2& barrier) { m_LastBarrier = barrier; }

    private:
        VkImage m_Image;
        VmaAllocation m_Allocation;
        Desc m_Desc;
        VkImageMemoryBarrier2 m_LastBarrier;
    };

    class ImageView
    {
    public:
        struct Desc
        {
            const aZero::RenderAPI::Image* Image;
            EImageAspect ImageAspect;
            uint32_t LayerStart = 0, LayerCount = 1u;
            uint32_t MipStart = 0, MipCount = 1u;
        };

        ImageView() = default;
        ImageView(const Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        void Init(const Core& core, const Desc& desc)
        {
            const Image::Desc& imageDesc = desc.Image->GetDesc();

            VkImageViewCreateInfo viewCI{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = desc.Image->Get(),
                .viewType = ToVK_ImageViewType(imageDesc.Type),
                .format = ToVK_ImageFormat(imageDesc.Format),
                .subresourceRange{.aspectMask = VkImageAspectFlags(desc.ImageAspect), .baseMipLevel = desc.MipStart, .levelCount = desc.MipCount, .baseArrayLayer = desc.LayerStart, .layerCount = desc.LayerCount}
            };

            chkSucceed(vkCreateImageView(core.GetLogicalDevice(), &viewCI, nullptr, &m_View));
        }

        VkImageView Get() const { return m_View; }

    private:
        VkImageView m_View;
    };
}