#include "CommandPool.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "RenderPass.hpp"

void aZero::RenderAPI::CommandBuffer::SetVertexBuffer(const RenderAPI::Buffer& buffer, uint64_t offset) const
{
    VkDeviceSize offsetTemp{ offset };
    std::vector<VkBuffer> buffers{ buffer.Get() };
    vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, buffers.data(), &offsetTemp);
}

void aZero::RenderAPI::CommandBuffer::SetIndexBuffer(const RenderAPI::Buffer& buffer, uint64_t offset, VkIndexType indexType) const
{
    VkDeviceSize offsetTemp{ offset };
    vkCmdBindIndexBuffer(m_CommandBuffer, buffer.Get(), offsetTemp, indexType);
}

void aZero::RenderAPI::CommandBuffer::Barrier(const std::vector<VkMemoryBarrier2>& globalBarriers, const std::vector<VkImageMemoryBarrier2>& imageBarriers, const std::vector<VkBufferMemoryBarrier2>& bufferBarriers) const
{
    VkDependencyInfo barrierDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = (uint32_t)globalBarriers.size(),
        .pMemoryBarriers = globalBarriers.data(),
        .bufferMemoryBarrierCount = (uint32_t)bufferBarriers.size(),
        .pBufferMemoryBarriers = bufferBarriers.data(),
        .imageMemoryBarrierCount = (uint32_t)imageBarriers.size(),
        .pImageMemoryBarriers = imageBarriers.data()
    };
    vkCmdPipelineBarrier2(m_CommandBuffer, &barrierDependencyInfo);
}

void aZero::RenderAPI::CommandBuffer::Copy(const BufferToImageCopyDesc& copyDesc, const RenderAPI::Buffer& srcBuffer, const RenderAPI::Image& dstImage)
{
    std::vector<VkBufferImageCopy> copyRegionsTemp{};
    for (uint32_t j = 0; j < copyDesc.MipOffsets.size(); j++) {
        copyRegionsTemp.push_back({
            .bufferOffset = copyDesc.MipOffsets[j],
            .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = j, .layerCount = 1},
            .imageExtent{.width = copyDesc.BaseWidth >> j, .height = copyDesc.BaseHeight >> j, .depth = copyDesc.DepthOrArrayCount },
            });
    }
    vkCmdCopyBufferToImage(m_CommandBuffer, srcBuffer.Get(), dstImage.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegionsTemp.size()), copyRegionsTemp.data());
}