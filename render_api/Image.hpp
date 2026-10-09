#pragma once
#include "CommandPool.hpp"
#include "Buffer.hpp"
#include "CommandQueue.hpp"

namespace aZero::RenderAPI
{
    class Image : public DeleteCopy
    {
    public:
        struct Desc
        {
            VkFormat Format;
            VkImageType Type;
            VkImageUsageFlags Usage;
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

        bool Init(Core& core, const Desc& desc)
        {
            VkImageCreateInfo depthImageCI{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = desc.Type,
                .format = desc.Format,
                .extent = {.width = desc.Width, .height = desc.Height, .depth = desc.Depth },
                .mipLevels = desc.MipCount,
                .arrayLayers = desc.ArrayLayers,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = desc.Usage,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };

            VmaAllocationCreateInfo allocCI{
                .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO
            };

            const bool succeeded = chkSucceed(vmaCreateImage(core.GetVmaAllocator(), &depthImageCI, &allocCI, &m_Image, &m_Allocation, nullptr));

            if (succeeded)
            {
                m_LastBarrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = 0,
                .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask = VK_ACCESS_2_NONE,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .image = m_Image,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_NONE, .levelCount = 1, .layerCount = 1 }
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
            
            return succeeded;
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
            VkImageViewType Type;
            VkImageAspectFlags ImageAspect = (VkImageAspectFlags)0;
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

        bool Init(Core& core, const Desc& desc)
        {
            const Image::Desc& imageDesc = desc.Image->GetDesc();

            VkImageViewCreateInfo viewCI{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = desc.Image->Get(),
                .viewType = desc.Type,
                .format = imageDesc.Format,
                .subresourceRange{.aspectMask = VkImageAspectFlags(desc.ImageAspect), .baseMipLevel = desc.MipStart, .levelCount = desc.MipCount, .baseArrayLayer = desc.LayerStart, .layerCount = desc.LayerCount}
            };

            const bool succeeded = chkSucceed(vkCreateImageView(core.GetLogicalDevice(), &viewCI, nullptr, &m_View));
            if (succeeded)
            {
                m_diCore = &core;
                m_Desc = desc;
            }
            return succeeded;
        }

        VkImageView Get() const { return m_View; }

    private:
        Core* m_diCore = nullptr;
        VkImageView m_View;
        Desc m_Desc;
    };
}