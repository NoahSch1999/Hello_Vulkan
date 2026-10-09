#pragma once
#include "Core.hpp"
#include <optional>
#include <type_traits>

namespace aZero::RenderAPI
{
    //enum class EMemoryType { BestFit, VideoMemory, SystemMemory, Readback};

    /*enum class EBufferUsageBits : uint32_t {
        TransferSrc = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        TransferDst = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        Uniform = VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        Storage = VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        Index = VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        Vertex = VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        Indirect = VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        DeviceAddress = VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    };*/
    
    /*constexpr EBufferUsageBits operator|(EBufferUsageBits lhs, EBufferUsageBits rhs)
    {
        return static_cast<EBufferUsageBits>(
            static_cast<std::uint64_t>(lhs) |
            static_cast<std::uint64_t>(rhs));
    }

    constexpr EBufferUsageBits& operator|=(EBufferUsageBits& lhs, EBufferUsageBits rhs)
    {
        using T = std::underlying_type_t<EBufferUsageBits>;

        lhs = static_cast<EBufferUsageBits>(
            static_cast<T>(lhs) | static_cast<T>(rhs)
            );

        return lhs;
    }

    inline VmaMemoryUsage ToVmaMemoryType(EMemoryType type)
    {
        switch (type)
        {
        case EMemoryType::BestFit:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
        case EMemoryType::VideoMemory:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
        case EMemoryType::SystemMemory:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY;
        case EMemoryType::Readback:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_TO_CPU;
        default:
            return VmaMemoryUsage::VMA_MEMORY_USAGE_MAX_ENUM;
        }
    }*/

    class Buffer : public DeleteCopy
    {
    public:
        struct Desc
        {
            uint64_t Size;
            VkBufferUsageFlags UsageBits;
            VmaMemoryUsage MemoryType;
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
            std::swap(m_LastBarrier, other.m_LastBarrier);
            return *this;
        }

        Buffer(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        bool Init(Core& core, const Desc& desc)
        {
            VkDeviceSize size{ desc.Size };

            VkBufferCreateInfo bufferCI{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = (VkBufferUsageFlags)desc.UsageBits
            };

            // todo: Make flags setting in combo with memory type
            VmaAllocationCreateInfo allocCI{
                .flags = desc.SeperateHeap ? (VmaAllocationCreateFlags)(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT) 
                    : (VmaAllocationCreateFlags)(VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT),
                .usage = desc.MemoryType
            };

            const bool succeeded = chkSucceed(vmaCreateBuffer(core.GetVmaAllocator(), &bufferCI, &allocCI, &m_Buffer, &m_Allocation, &m_AllocationInfo));

            if (succeeded)
            {
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

                m_LastBarrier = {};

                m_Desc = desc;
                m_diCore = &core;
            }

            return succeeded;
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
            m_LastBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
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