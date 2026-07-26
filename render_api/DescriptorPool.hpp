#pragma once
#include "Core.hpp"
#include "Sampler.hpp"
#include "Image.hpp"

namespace aZero::RenderAPI
{
    struct DescriptorHandle
    {
        RenderAPI::Sampler* Sampler;
        RenderAPI::ImageView* View;
        VkImageLayout Layout;
        uint32_t Index;
    };

    // todo Make it support other type of descriptors than VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    class DescriptorPool
    {
    public:
        struct Desc
        {
            //std::vector<VkDescriptorPoolSize> PoolTypes;
            uint32_t Count;
        };

        DescriptorPool() = default;
        DescriptorPool(const Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        void Init(const Core& core, const Desc& desc)
        {
            VkDescriptorPoolSize poolSize{
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // Combined. But maybe i should change to seperate for samplers and textures?
                .descriptorCount = desc.Count
            };

            VkDescriptorPoolCreateInfo descPoolCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = 1,
                .poolSizeCount = 1,
                .pPoolSizes = &poolSize
            };
            chkSucceed(vkCreateDescriptorPool(core.GetLogicalDevice(), &descPoolCI, nullptr, &m_DescriptorPool));

            VkDescriptorBindingFlags descVariableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
            VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = desc.Count,
                .pBindingFlags = &descVariableFlag
            };
            VkDescriptorSetLayoutBinding descLayoutBindingTex{
                .descriptorType = poolSize.type,
                .descriptorCount = desc.Count,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            };
            VkDescriptorSetLayoutCreateInfo descLayoutTexCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &descBindingFlags,
                .bindingCount = 1,
                .pBindings = &descLayoutBindingTex
            };
            chkSucceed(vkCreateDescriptorSetLayout(core.GetLogicalDevice(), &descLayoutTexCI, nullptr, &m_DescriptorImageLayout));

            uint32_t variableDescCount{ desc.Count };
            VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
                .descriptorSetCount = desc.Count,
                .pDescriptorCounts = &variableDescCount
            };
            VkDescriptorSetAllocateInfo texDescSetAlloc{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = &variableDescCountAI,
                .descriptorPool = m_DescriptorPool,
                .descriptorSetCount = desc.Count,
                .pSetLayouts = &m_DescriptorImageLayout
            };
            chkSucceed(vkAllocateDescriptorSets(core.GetLogicalDevice(), &texDescSetAlloc, &m_DescriptorImageSet));

            m_Desc = desc;
        }

        void UploadDescriptor(const Core& core, const DescriptorHandle& handle)
        {
            VkDescriptorImageInfo textureDescriptor{ .sampler = handle.Sampler->Get(), .imageView = handle.View->Get(), .imageLayout = handle.Layout};

            VkWriteDescriptorSet writeDescSet{ // used to update a descriptor in a descriptor set
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_DescriptorImageSet,
                .dstBinding = 0,
                .dstArrayElement = handle.Index,
                .descriptorCount = static_cast<uint32_t>(1),
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &textureDescriptor
            };
            vkUpdateDescriptorSets(core.GetLogicalDevice(), 1, &writeDescSet, 0, nullptr);
        }

        VkDescriptorPool Get() const { return m_DescriptorPool; }
        VkDescriptorSet GetSet() const { return m_DescriptorImageSet; }
        VkDescriptorSetLayout GetLayout() const { return m_DescriptorImageLayout; }

    private:
        VkDescriptorPool m_DescriptorPool;
        VkDescriptorSet m_DescriptorImageSet;
        VkDescriptorSetLayout m_DescriptorImageLayout;
        Desc m_Desc;
    };
}