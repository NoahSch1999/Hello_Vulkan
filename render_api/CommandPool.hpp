#pragma once
#include "Core.hpp"

namespace aZero::RenderAPI
{
    class Buffer;
    class Image;

    struct BufferToImageCopyDesc
    {
        uint32_t BaseWidth, BaseHeight, DepthOrArrayCount;
        std::vector<uint32_t> MipOffsets;
    };

    class Binding
    {
        friend class CommandBuffer;
    public:
        Binding() = default;
        Binding(const VkPushConstantRange& pcBinding, VkPipelineLayout layout)
            :BindingInfo(pcBinding), Layout(layout) {}
        bool IsValid() const { return Layout != NULL; }
    private:
        VkPushConstantRange BindingInfo;
        VkPipelineLayout Layout = NULL;
    };

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

        void DispatchMesh(uint32_t tgx, uint32_t tgy, uint32_t tgz)
        {
            vkCmdDrawMeshTasksEXT(m_CommandBuffer, tgx, tgy, tgz);
        }

        void SetScizzorRectsAndViewports(const std::vector<VkViewport>& viewports, const std::vector<VkRect2D>& scizzorRects) const
        {
            vkCmdSetViewport(m_CommandBuffer, 0, viewports.size(), viewports.data());
            vkCmdSetScissor(m_CommandBuffer, 0, scizzorRects.size(), scizzorRects.data());
        }

        void SetPipeline() const
        {

        }

        void SetVertexBuffer(const RenderAPI::Buffer& buffer, uint64_t offset) const;

        void SetIndexBuffer(const RenderAPI::Buffer& buffer, uint64_t offset, VkIndexType indexType = VK_INDEX_TYPE_UINT32) const;

        template<typename T>
        void SetPushConstants(const Binding& binding, const T& data) const
        {
            if (binding.IsValid())
            {
                vkCmdPushConstants(m_CommandBuffer, binding.Layout, binding.BindingInfo.stageFlags, binding.BindingInfo.offset, binding.BindingInfo.size, &data);
            }
        }

        void Barrier(const std::vector<VkMemoryBarrier2>& globalBarriers, const std::vector<VkImageMemoryBarrier2>& imageBarriers, const std::vector<VkBufferMemoryBarrier2>& bufferBarriers) const;

        void Copy(const BufferToImageCopyDesc& copyDesc, const RenderAPI::Buffer& srcBuffer, const RenderAPI::Image& dstImage);

        VkCommandBuffer Get() const { return m_CommandBuffer; }

    private:
        CommandBuffer(Core& core, VkCommandBuffer commandBuffer, EQueueType type)
            :m_diCore(&core), m_CommandBuffer(commandBuffer), m_Type(type) {  }

        RenderAPI::Core* m_diCore = nullptr;
        VkCommandBuffer m_CommandBuffer;
        EQueueType m_Type;
    };

    class CommandPool : public DeleteCopy
    {
    public:
        struct Desc
        {
            EQueueType QueueType;
        };

        CommandPool() = default;
        ~CommandPool()
        {
            if (m_diCore)
            {
                vkDestroyCommandPool(m_diCore->GetLogicalDevice(), m_CommandPool, nullptr);
            }
        }

        CommandPool(CommandPool&& other) noexcept
        {
            *this = std::move(other);
        }

        CommandPool& operator=(CommandPool&& other) noexcept
        {
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_CommandPool, other.m_CommandPool);
            std::swap(m_Desc, other.m_Desc);
            return *this;
        }

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