#pragma once
#include "Core.hpp"
#include "Buffer.hpp"

namespace aZero::RenderAPI
{
    class CommandBuffer
    {
        friend class CommandPool;
    public:
        CommandBuffer() = default;

        void StartRecording(VkCommandBufferUsageFlags flags) const
        {
            VkCommandBufferBeginInfo cbBI{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = flags
            };
            chkSucceed(vkBeginCommandBuffer(m_CommandBuffer, &cbBI));
        }

        void StopRecording() const
        {
            vkEndCommandBuffer(m_CommandBuffer);
        }

        void Reset() const
        {
            chkSucceed(vkResetCommandBuffer(m_CommandBuffer, 0));
        }

        void BeginRender(const VkRenderingInfo& renderInfo) const
        {
            vkCmdBeginRendering(m_CommandBuffer, &renderInfo);
        }

        void EndRender() const
        {
            vkCmdEndRendering(m_CommandBuffer);
        }

        struct DrawIndexedDesc
        {
            uint32_t IndexCount = 0;
            uint32_t InstanceCount = 0;
            uint32_t FirstIndex = 0;
            int32_t  VertexOffset = 0;
            uint32_t FirstInstance = 0;
        };

        void DrawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount,
            uint32_t firstIndex,
            int32_t  vertexOffset,
            uint32_t firstInstance) const
        {
            vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }

        void SetScizzorRectsAndViewports(const std::vector<VkViewport>& viewports, const std::vector<VkRect2D>& scizzorRects) const
        {
            vkCmdSetViewport(m_CommandBuffer, 0, viewports.size(), viewports.data());
            vkCmdSetScissor(m_CommandBuffer, 0, scizzorRects.size(), scizzorRects.data());
        }

        void SetPipeline() const
        {

        }

        void SetVertexBuffer(const RenderAPI::Buffer& buffer, uint64_t offset) const
        {
            VkDeviceSize offsetTemp{ offset };
            std::vector<VkBuffer> buffers{ buffer.Get() };
            vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, buffers.data(), &offsetTemp);
        }

        void SetIndexBuffer(const RenderAPI::Buffer& buffer, uint64_t offset, VkIndexType indexType = VK_INDEX_TYPE_UINT32) const
        {
            VkDeviceSize offsetTemp{ offset };
            vkCmdBindIndexBuffer(m_CommandBuffer, buffer.Get(), offsetTemp, indexType);
        }

        void SetPushConstants() const
        {

        }

        void Barrier(const std::vector<VkImageMemoryBarrier2>& barriers) const
        {
            VkDependencyInfo barrierDependencyInfo{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .imageMemoryBarrierCount = (uint32_t)barriers.size(),
                .pImageMemoryBarriers = barriers.data()
            };
            vkCmdPipelineBarrier2(m_CommandBuffer, &barrierDependencyInfo);
        }

        VkCommandBuffer Get() const { return m_CommandBuffer; }

    private:
        CommandBuffer(Core& core, VkCommandBuffer commandBuffer, EQueueType type)
            :m_diCore(&core), m_CommandBuffer(commandBuffer), m_Type(type) {  }

        RenderAPI::Core* m_diCore = nullptr;
        VkCommandBuffer m_CommandBuffer;
        EQueueType m_Type;
    };

    class CommandPool
    {
    public:
        struct Desc
        {
            EQueueType QueueType;
        };

        CommandPool() = default;
        CommandPool(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        void Init(Core& core, const Desc& desc)
        {
            VkCommandPoolCreateInfo cmdPoolCI{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = core.GetIndexForQueueType(desc.QueueType)
            };

            chkSucceed(vkCreateCommandPool(core.GetLogicalDevice(), &cmdPoolCI, nullptr, &m_CommandPool));

            m_diCore = &core;
            m_Desc = desc;
        }

        std::vector<CommandBuffer> CreateCommandBuffers(const Core& core, uint32_t count)
        {
            std::vector<VkCommandBuffer> commandBuffers(count);
            VkCommandBufferAllocateInfo cmdBufferInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = m_CommandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = count
            };

            chkSucceed(vkAllocateCommandBuffers(core.GetLogicalDevice(), &cmdBufferInfo, commandBuffers.data()));

            std::vector<CommandBuffer> outp(count);
            for (uint32_t i = 0; i < count; i++) {
                outp[i] = CommandBuffer(*m_diCore, commandBuffers[i], m_Desc.QueueType);
            }
            return outp;
        }

        VkCommandPool Get() const { return m_CommandPool; }

    private:
        RenderAPI::Core* m_diCore = nullptr;
        VkCommandPool m_CommandPool;
        Desc m_Desc;
    };
}