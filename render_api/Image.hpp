#pragma once
#include "CommandPool.hpp"
#include "Buffer.hpp"
#include "CommandQueue.hpp"

namespace aZero::RenderAPI
{

    using EImageFormat = VkFormat;

    enum class EImageType : std::uint32_t
    {
        Image1D,
        Image1D_Array,
        Image2D,
        Image2D_Array,
        ImageCube,
        ImageCube_Array,
        Image3D
    };

    enum class EImageUsageBits : VkImageUsageFlags
    {
        TransferSrc = VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        TransferDst = VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        Sampled = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
        Storage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT,
        Color_Attachment = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        DepthStencil_Attachment = VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    };

    constexpr EImageUsageBits operator|(EImageUsageBits lhs, EImageUsageBits rhs)
    {
        return static_cast<EImageUsageBits>(
            static_cast<std::uint32_t>(lhs) |
            static_cast<std::uint32_t>(rhs));
    };

    enum EImageAspect : VkImageAspectFlags
    {
        Color = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
        Depth = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT,
        Stencil = VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT,
        DepthStencil = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT
    };

    constexpr EImageAspect operator|(EImageAspect lhs, EImageAspect rhs)
    {
        return static_cast<EImageAspect>(
            static_cast<std::uint32_t>(lhs) |
            static_cast<std::uint32_t>(rhs));
    }

    // IF VULKAN!
    inline VkFormat ToVK_ImageFormat(EImageFormat format)
    {
        return format;
        /*switch (format)
        {
        case EImageFormat::RGBA8_UNORM:
            return VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
        case EImageFormat::RGBA8_UNORM_SRGB:
            return VkFormat::VK_FORMAT_R8G8B8A8_SRGB;
        case EImageFormat::BGRA8_UNORM:
            return VkFormat::VK_FORMAT_B8G8R8A8_UNORM;
        case EImageFormat::BGRA8_UNORM_SRGB:
            return VkFormat::VK_FORMAT_B8G8R8A8_SRGB;
        case EImageFormat::BC7_UNORM:
            return VkFormat::VK_FORMAT_BC7_UNORM_BLOCK;
        case EImageFormat::BC7_UNORM_SRGB:
            return VkFormat::VK_FORMAT_BC7_SRGB_BLOCK;
        case EImageFormat::D24S8_UINT:
            return VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
        default:
            return VkFormat::VK_FORMAT_UNDEFINED;
        }*/
    }

    inline VkImageType ToVK_ImageType(EImageType type) // todo: Get rid of this
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

    inline VkImageViewType ToVK_ImageViewType(EImageType type) // todo: Get rid of this
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

    class Image : public DeleteCopy
    {
    public:
        struct Desc
        {
            EImageFormat Format;
            EImageType Type;
            EImageUsageBits Usage;
            uint32_t Width = 0u, Height = 0u, Depth = 0u;
            uint32_t MipCount = 1u;
            uint32_t ArrayLayers = 1u;
            std::string Name;
        };

        Image() = default;
        ~Image()
        {
            if (m_diCore)
            {
                vmaDestroyImage(m_diCore->GetVmaAllocator(), m_Image, m_Allocation);
            }
        }

        Image(Image&& other) noexcept
        {
            *this = std::move(other);
        }

        Image& operator=(Image&& other) noexcept
        {
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_Image, other.m_Image);
            std::swap(m_Allocation, other.m_Allocation);
            std::swap(m_LastBarrier, other.m_LastBarrier);
            std::swap(m_Desc, other.m_Desc);
            return *this;
        }

        Image(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        // todo: Impl fail-safe behaviour
        void Init(Core& core, const Desc& desc)
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

            if (!desc.Name.empty() && core.IsDebugNamesSupported())
            {
                VkDebugUtilsObjectNameInfoEXT nameInfo{};
                nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
                nameInfo.objectHandle = (uint64_t)m_Image;
                nameInfo.pObjectName = desc.Name.c_str();

                const VkResult enableDebugNamesRes = vkSetDebugUtilsObjectNameEXT(core.GetLogicalDevice(), &nameInfo);
                if (enableDebugNamesRes != VK_SUCCESS)
                {
                    std::cout << "Failed to enable debug name extension.\n";
                }
            }

            m_diCore = &core;
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

        void Upload(Core& core, CommandQueue& commandQueue, CommandPool& commandPool, Buffer& stagingBuffer, const BufferToImageCopyDesc& uploadDesc)
        {
            RenderAPI::Fence fenceOneTime(core);

            RenderAPI::CommandBuffer cbOneTime = commandPool.CreateCommandBuffers(core, 1)[0];
            cbOneTime.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

            this->Transition({
                .DstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .SubresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = (uint32_t)uploadDesc.MipOffsets.size(), .layerCount = 1}
            });
            cbOneTime.Barrier({}, { m_LastBarrier }, {});

            cbOneTime.Copy(uploadDesc, stagingBuffer, *this);

            this->Transition({
                .DstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                .NewLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                .SubresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = (uint32_t)uploadDesc.MipOffsets.size(), .layerCount = 1}
            });
            cbOneTime.Barrier({}, { m_LastBarrier }, {});

            cbOneTime.StopRecording();

            RenderAPI::CommandQueue::SubmitDesc submitDesc{
            .CommandBufferPtr = &cbOneTime,
            .OnCompletionFence = &fenceOneTime
            };

            commandQueue.SubmitCommandBuffers(submitDesc);

            fenceOneTime.WaitFor();
        }

        VkImage Get() const { return m_Image; }
        const Desc& GetDesc() const { return m_Desc; }
        const VkImageMemoryBarrier2& GetTransition() const { return m_LastBarrier; }
        void OverrideInternalMemoryBarrier(const VkImageMemoryBarrier2& barrier) { m_LastBarrier = barrier; }

    private:
        Core* m_diCore = nullptr;
        VkImage m_Image;
        VmaAllocation m_Allocation;
        Desc m_Desc;
        VkImageMemoryBarrier2 m_LastBarrier;
    };

    class ImageView : public DeleteCopy
    {
    public:
        struct Desc
        {
            const aZero::RenderAPI::Image* Image = nullptr;
            EImageAspect ImageAspect = (EImageAspect)0;
            uint32_t LayerStart = 0, LayerCount = 1u;
            uint32_t MipStart = 0, MipCount = 1u;
        };

        ImageView() = default;
        ~ImageView()
        {
            if (m_diCore)
            {
                vkDestroyImageView(m_diCore->GetLogicalDevice(), m_View, nullptr);
            }
        }

        ImageView(ImageView&& other) noexcept
        {
            *this = std::move(other);
        }

        ImageView& operator=(ImageView&& other) noexcept
        {
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_View, other.m_View);
            std::swap(m_Desc, other.m_Desc);
            return *this;
        }

        ImageView(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        void Init(Core& core, const Desc& desc)
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

            m_diCore = &core;
            m_Desc = desc;
        }

        VkImageView Get() const { return m_View; }

    private:
        Core* m_diCore = nullptr;
        VkImageView m_View;
        Desc m_Desc;
    };
}