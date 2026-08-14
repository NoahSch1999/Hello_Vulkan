#pragma once
#include "Fence.hpp"
#include "Semaphore.hpp"
#include "CommandPool.hpp"
#include "Buffer.hpp"
#include "Image.hpp"

namespace aZero::RenderAPI
{
    class CommandQueue
    {
    public:
        CommandQueue() = default;
        CommandQueue(Core& core, EQueueType type)
        {
            this->Init(core, type);
        }

        void Init(Core& core, EQueueType type)
        {
            vkGetDeviceQueue(core.GetLogicalDevice(), core.GetIndexForQueueType(type), 0, &m_Queue);
            m_diCore = &core;
        }

        VkQueue Get() const { return m_Queue; }

        struct SubmitDesc
        {
            struct SubmitSemaphore
            {
                Semaphore* SemaphorePtr;
                VkPipelineStageFlags2 Stage;
                uint64_t Value;
            };

            RenderAPI::CommandBuffer* CommandBufferPtr = nullptr;
            Fence* OnCompletionFence;

            std::vector<SubmitSemaphore> WaitOnSemaphores; // Signalled when the stage has been reached
            std::vector<SubmitSemaphore> SignalSemaphores; // Signalled when the stage has
        };

        // todo Impl foo's
        void SubmitCommandBuffers(const SubmitDesc& submitDesc)
        {
            std::vector<VkSemaphoreSubmitInfo> WaitOnSemaphores_BinaryVec(submitDesc.WaitOnSemaphores.size());
            for (uint32_t i = 0; i < WaitOnSemaphores_BinaryVec.size(); i++)
            {
                WaitOnSemaphores_BinaryVec[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = submitDesc.WaitOnSemaphores[i].SemaphorePtr->Get(), // this ensured that we wait to start the stageMask stage until this semaphore has been signalled (since its used as pWaitSemaphoreInfos) before executing further commands in the buffer
                    .stageMask = submitDesc.WaitOnSemaphores[i].Stage  /*VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT*/ // this makes the wait happen on the color attachment stage, so prior work can still be done
                };

                if (submitDesc.WaitOnSemaphores[i].SemaphorePtr->GetType() == RenderAPI::ESemaphoreType::Timeline)
                {
                    WaitOnSemaphores_BinaryVec[i].value = submitDesc.WaitOnSemaphores[i].Value;
                }
            }

            std::vector<VkSemaphoreSubmitInfo> SignalSemaphores_BinaryVec(submitDesc.SignalSemaphores.size());
            for (uint32_t i = 0; i < SignalSemaphores_BinaryVec.size(); i++)
            {
                SignalSemaphores_BinaryVec[i] = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = submitDesc.SignalSemaphores[i].SemaphorePtr->Get(), // this is signalled when the command buffer has passed the stageMask point (since its used as pSignalSemaphoreInfos)
                    .stageMask = submitDesc.SignalSemaphores[i].Stage /*VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,*/
                };

                if (submitDesc.SignalSemaphores[i].SemaphorePtr->GetType() == RenderAPI::ESemaphoreType::Timeline)
                {
                    SignalSemaphores_BinaryVec[i].value = submitDesc.SignalSemaphores[i].Value;
                }
            }

            VkCommandBufferSubmitInfo commandBufferSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = submitDesc.CommandBufferPtr->Get()
            };

            VkSubmitInfo2 submitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .waitSemaphoreInfoCount = (uint32_t)WaitOnSemaphores_BinaryVec.size(),
                .pWaitSemaphoreInfos = WaitOnSemaphores_BinaryVec.data(),
                .commandBufferInfoCount = 1u,
                .pCommandBufferInfos = &commandBufferSubmitInfo,
                .signalSemaphoreInfoCount = (uint32_t)SignalSemaphores_BinaryVec.size(),
                .pSignalSemaphoreInfos = SignalSemaphores_BinaryVec.data()
            };

            chkSucceed(vkQueueSubmit2(m_Queue, 1, &submitInfo, submitDesc.OnCompletionFence->Get()));
        }

    private:
        RenderAPI::Core* m_diCore = nullptr;
        VkQueue m_Queue; 
        EQueueType m_Type;
    };
}