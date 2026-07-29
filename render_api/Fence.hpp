#pragma once
#include "Core.hpp"

namespace aZero::RenderAPI
{
    class Fence
    {
    public:
        Fence() = default;
        Fence(Core& core)
        {
            this->Init(core);
        }

        void Init(Core& core)
        {
            VkFenceCreateInfo fenceCI{
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT
            };
            chkSucceed(vkCreateFence(core.GetLogicalDevice(), &fenceCI, nullptr, &m_Fence));

            m_diCore = &core;
        }

        void WaitFor(uint64_t timeout = UINT64_MAX)
        {
            chkSucceed(vkWaitForFences(m_diCore->GetLogicalDevice(), 1, &m_Fence, true, timeout));
        }

        static void WaitForMultiple(const RenderAPI::Core& core, const std::vector<Fence>& fences, bool waitForAll = true, uint64_t timeout = UINT64_MAX)
        {
            std::vector<VkFence> fencesVk(fences.size());
            for (uint32_t i = 0; i < fencesVk.size(); i++) {
                fencesVk[i] = fences[i].Get();
            }
            chkSucceed(vkWaitForFences(core.GetLogicalDevice(), fences.size(), fencesVk.data(), true, timeout));
        }

        void Reset()
        {
            chkSucceed(vkResetFences(m_diCore->GetLogicalDevice(), 1, &m_Fence));
        }

        static void ResetMultiple(const RenderAPI::Core& core, const std::vector<Fence>& fences)
        {
            std::vector<VkFence> fencesVk(fences.size());
            for (uint32_t i = 0; i < fencesVk.size(); i++) {
                fencesVk[i] = fences[i].Get();
            }
            chkSucceed(vkResetFences(core.GetLogicalDevice(), fencesVk.size(), fencesVk.data()));
        }

        VkFence Get() const { return m_Fence; }

        // todo Impl foo's

    private:
        RenderAPI::Core* m_diCore = nullptr;
        VkFence m_Fence;
    };
}