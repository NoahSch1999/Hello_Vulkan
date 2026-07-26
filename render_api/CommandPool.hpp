#pragma once
#include "Core.hpp"

namespace aZero::RenderAPI
{
    class CommandPool
    {
    public:
        struct Desc
        {
            EQueueType QueueType;
        };

        CommandPool() = default;
        CommandPool(const Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        void Init(const Core& core, const Desc& desc)
        {
            VkCommandPoolCreateInfo cmdPoolCI{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = core.GetIndexForQueueType(desc.QueueType)
            };

            chkSucceed(vkCreateCommandPool(core.GetLogicalDevice(), &cmdPoolCI, nullptr, &m_CommandPool));

            m_Desc = desc;
        }

        std::vector<VkCommandBuffer> CreateCommandBuffers(const Core& core, uint32_t count)
        {
            std::vector<VkCommandBuffer> commandBuffers(count);
            VkCommandBufferAllocateInfo cmdBufferInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = m_CommandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = count
            };

            chkSucceed(vkAllocateCommandBuffers(core.GetLogicalDevice(), &cmdBufferInfo, commandBuffers.data()));
            return commandBuffers;
        }

        VkCommandPool Get() const { return m_CommandPool; }

    private:
        VkCommandPool m_CommandPool;
        Desc m_Desc;
    };
}