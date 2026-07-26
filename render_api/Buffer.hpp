#pragma once
#include "Core.hpp"

namespace aZero::RenderAPI
{
    class Buffer
    {
    public:
        struct Desc
        {
            uint64_t Size;
            VkBufferUsageFlags AccessUsage;
            VmaAllocationCreateFlags Flags;
            VmaMemoryUsage MemoryUsage;
        };

        Buffer() = default;
        Buffer(const Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        void Init(const Core& core, const Desc& desc)
        {
            VkDeviceSize size{ desc.Size };
            VkBufferCreateInfo bufferCI{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = desc.AccessUsage /*VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT*/
            };

            VmaAllocationCreateInfo allocCI{
                .flags = desc.Flags /*VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT*/,
                .usage = desc.MemoryUsage /*VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO*/
            };

            chkSucceed(vmaCreateBuffer(core.GetVmaAllocator(), &bufferCI, &allocCI, &m_Buffer, &m_Allocation, &m_AllocationInfo));

            if (bufferCI.usage & VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            {
                VkBufferDeviceAddressInfo bdaInfo{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                    .buffer = m_Buffer
                };
                m_DeviceAddress = vkGetBufferDeviceAddress(core.GetLogicalDevice(), &bdaInfo);
            }

            m_Desc = desc;
        }

        void Upload(void* data, size_t size, size_t dstOffset)
        {
            memcpy(((char*)this->GetMappedPtr()) + dstOffset, data, size);
        }

        VkBuffer Get() const { return m_Buffer; }
        Desc GetDesc() const { return m_Desc; }
        void* GetMappedPtr() const { return m_AllocationInfo.pMappedData; }
        std::optional<VkDeviceAddress> GetDeviceAddress() const { return m_DeviceAddress; }

    private:
        VkBuffer m_Buffer;
        VmaAllocation m_Allocation;
        VmaAllocationInfo m_AllocationInfo;
        std::optional<VkDeviceAddress> m_DeviceAddress;
        Desc m_Desc;
    };
}