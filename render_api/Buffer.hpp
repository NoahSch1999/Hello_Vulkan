#pragma once
#include "Core.hpp"
#include <optional>

namespace aZero::RenderAPI
{
    enum class EMemoryType { BestFit, VRAM_Only, SYSRAM_Only, Readback };
    enum class EBufferUsageBits { TransferSrc = 0x00000001, TransferDst = 0x00000002, Uniform = 0x00000010, Storage = 0x00000020, Index = 0x00000040, Vertex = 0x00000080, Indirect = 0x00000100, DeviceAddress = 0x00020000 };
    
    constexpr EBufferUsageBits operator|(EBufferUsageBits lhs, EBufferUsageBits rhs)
    {
        return static_cast<EBufferUsageBits>(
            static_cast<std::uint32_t>(lhs) |
            static_cast<std::uint32_t>(rhs));
    }

    inline VmaMemoryUsage ToVmaMemoryType(EMemoryType type)
    {
        switch (type)
        {
        case EMemoryType::BestFit:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
        case EMemoryType::VRAM_Only:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
        case EMemoryType::SYSRAM_Only:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY;
        case EMemoryType::Readback:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_TO_CPU;
        default:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_MAX_ENUM;
        }
    }

    inline VkBufferUsageFlagBits ToVkBufferUsage(EBufferUsageBits type)
    {
        switch (type)
        {
        case EBufferUsageBits::TransferSrc:
            return VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case EBufferUsageBits::TransferDst:
            return VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        case EBufferUsageBits::Uniform:
            return VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case EBufferUsageBits::Storage:
            return VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case EBufferUsageBits::Index:
            return VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case EBufferUsageBits::Vertex:
            return VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case EBufferUsageBits::Indirect:
            return VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        case EBufferUsageBits::DeviceAddress:
            return VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        default:
            return VkBufferUsageFlagBits::VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
        }
    }

    class Buffer : public DeleteCopy
    {
    public:
        struct Desc
        {
            uint64_t Size;
            EBufferUsageBits UsageBits;
            EMemoryType MemoryType = EMemoryType::BestFit;
            bool SeperateHeap = false;
            std::string Name;
        };

        Buffer() = default;
        ~Buffer()
        {
            if (m_diCore)
            {
                vmaDestroyBuffer(m_diCore->GetVmaAllocator(), m_Buffer, m_Allocation);
            }
        }

        Buffer(Buffer&& other) noexcept
        {
            *this = std::move(other);
        }

        Buffer& operator=(Buffer&& other) noexcept
        {
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_Buffer, other.m_Buffer);
            std::swap(m_Allocation, other.m_Allocation);
            std::swap(m_AllocationInfo, other.m_AllocationInfo);
            std::swap(m_DeviceAddress, other.m_DeviceAddress);
            std::swap(m_Desc, other.m_Desc);
            return *this;
        }

        Buffer(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        void Init(Core& core, const Desc& desc)
        {
            VkDeviceSize size{ desc.Size };
            VkBufferCreateInfo bufferCI{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = (VkBufferUsageFlags)desc.UsageBits /*VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT*/
            };

            VmaAllocationCreateInfo allocCI{
                .flags = desc.SeperateHeap ? (VmaAllocationCreateFlags)(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) 
                    : (VmaAllocationCreateFlags)(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT),
                .usage = ToVmaMemoryType(desc.MemoryType) /*VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO*/
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

            if (!desc.Name.empty() && core.IsDebugNamesSupported())
            {
                VkDebugUtilsObjectNameInfoEXT nameInfo{};
                nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
                nameInfo.objectHandle = (uint64_t)m_Buffer;
                nameInfo.pObjectName = desc.Name.c_str();

                const VkResult enableDebugNamesRes = vkSetDebugUtilsObjectNameEXT(core.GetLogicalDevice(), &nameInfo);
                if (enableDebugNamesRes != VK_SUCCESS)
                {
                    std::cout << "Failed to enable debug name extension.\n";
                }
            }

            std::cout << desc.Name << ":\n";
            VkMemoryPropertyFlags props{};
            vmaGetAllocationMemoryProperties(
                core.GetVmaAllocator(),
                m_Allocation,
                &props
            );

            printf("memory props: 0x%x\n", props);
            printf("mapped: %p\n", m_AllocationInfo.pMappedData);

            m_Desc = desc;
            m_diCore = &core;
        }

        template<typename T> requires std::is_trivially_copyable_v<T>
        auto Upload(const T& data, uint32_t dstOffset)
        {
            memcpy(((char*)this->GetMappedPtr()) + dstOffset, (void*)&data, sizeof(T));
        }

        void Upload(void* data, size_t size, size_t dstOffset)
        {
            memcpy(((char*)this->GetMappedPtr()) + dstOffset, data, size);
        }

        template<typename T>
        void Upload(const std::vector<T>& data, size_t dstOffset)
        {
            memcpy(((char*)this->GetMappedPtr()) + dstOffset, data.data(), data.size() * sizeof(T));
        }

        struct TransitionDesc
        {
            VkPipelineStageFlags2 DstStageMask;
            VkAccessFlags2 DstAccessMask;
            uint32_t Offset, Size;
        };

        void Transition(const TransitionDesc& desc)
        {
            m_LastBarrier.srcStageMask = m_LastBarrier.dstStageMask;
            m_LastBarrier.srcAccessMask = m_LastBarrier.dstAccessMask;

            m_LastBarrier.dstStageMask = desc.DstStageMask;
            m_LastBarrier.dstAccessMask = desc.DstAccessMask;
            m_LastBarrier.buffer = m_Buffer;
            m_LastBarrier.offset = desc.Offset;
            m_LastBarrier.size = desc.Size;
        }

        VkBuffer Get() const { return m_Buffer; }
        Desc GetDesc() const { return m_Desc; }
        void* GetMappedPtr() const { return m_AllocationInfo.pMappedData; }
        std::optional<VkDeviceAddress> GetDeviceAddress() const { return m_DeviceAddress; }
        const VkBufferMemoryBarrier2& GetTransition() const { return m_LastBarrier; }

    private:
        Core* m_diCore = nullptr;
        VkBuffer m_Buffer;
        VmaAllocation m_Allocation;
        VmaAllocationInfo m_AllocationInfo;
        std::optional<VkDeviceAddress> m_DeviceAddress;
        Desc m_Desc;
        VkBufferMemoryBarrier2 m_LastBarrier;
    };
}