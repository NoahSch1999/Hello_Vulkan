#pragma once
#include "Core.hpp"

namespace aZero::RenderAPI
{
    enum class ESemaphoreType { Binary, Timeline };
    class Semaphore
    {
    public:
        Semaphore() = default;
        Semaphore(Core& core, ESemaphoreType type)
        {
            this->Init(core, type);
        }

        void Init(Core& core, ESemaphoreType type)
        {
            VkSemaphoreCreateInfo semaphoreCI{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
            };

            VkSemaphoreTypeCreateInfo timelineCreateInfo;
            if (type == ESemaphoreType::Timeline)
            {
                timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
                timelineCreateInfo.pNext = NULL;
                timelineCreateInfo.semaphoreType = VkSemaphoreType::VK_SEMAPHORE_TYPE_TIMELINE;
                timelineCreateInfo.initialValue = 0;

                semaphoreCI.pNext = &timelineCreateInfo;
            }
            
            chkSucceed(vkCreateSemaphore(core.GetLogicalDevice(), &semaphoreCI, nullptr, &m_Semaphore));

            m_Type = type;
            m_diCore = &core;
        }

        VkSemaphore Get() const { return m_Semaphore; }
        ESemaphoreType GetType() const { return m_Type; }
        // todo Impl foo's

        // -----------------------------------------------------------------
        // TIMELINE SEMAPHORE
        uint64_t Signal_Timeline()
        {
            if (m_Type != ESemaphoreType::Timeline)
            {
                return UINT64_MAX;
            }

            VkSemaphoreSignalInfo signalInfo;
            signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
            signalInfo.pNext = NULL;
            signalInfo.semaphore = m_Semaphore;
            signalInfo.value = m_LastSignal;

            m_LastSignal++;

            vkSignalSemaphore(m_diCore->GetLogicalDevice(), &signalInfo);

            return m_LastSignal++;
        }

        void Wait_Timeline(uint64_t waitValue, uint64_t timeout = UINT64_MAX)
        {
            if (m_Type != ESemaphoreType::Timeline)
            {
                return;
            }
            VkSemaphoreWaitInfo waitInfo;
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            waitInfo.pNext = NULL;
            waitInfo.flags = 0;
            waitInfo.semaphoreCount = 1;
            waitInfo.pSemaphores = &m_Semaphore;
            waitInfo.pValues = &waitValue;

            vkWaitSemaphores(m_diCore->GetLogicalDevice(), &waitInfo, UINT64_MAX);
        }

        uint64_t GetLatestSignal_Timeline() const {
            if (m_Type != ESemaphoreType::Timeline)
            {
                return UINT64_MAX;
            }
            return m_LastSignal; 
        }

        uint64_t GetActualValue_Timeline() const {
            if (m_Type != ESemaphoreType::Timeline)
            {
                return UINT64_MAX;
            }

            uint64_t current;
            vkGetSemaphoreCounterValue(m_diCore->GetLogicalDevice(), m_Semaphore, &current);
            return current;
        }
        // -----------------------------------------------------------------

    private:
        RenderAPI::Core* m_diCore = nullptr;
        VkSemaphore m_Semaphore;
        ESemaphoreType m_Type;
        uint64_t m_LastSignal = 0;
    };
}