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

    class DescriptorSet
    {
        friend class DescriptorPool;
    public:
        struct Desc
        {
            VkDescriptorType Type;
            uint32_t DescriptorCount;
            VkShaderStageFlags ShaderStages;
        };

        DescriptorSet() = default;

        void UpdateDescriptor(const DescriptorHandle& handle)
        {
            VkDescriptorImageInfo descriptor{ .sampler = handle.Sampler->Get(), .imageView = handle.View->Get(), .imageLayout = handle.Layout };

            VkWriteDescriptorSet writeDescSet{ // used to update a descriptor in a descriptor set
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_Set,
                .dstBinding = 0,
                .dstArrayElement = handle.Index,
                .descriptorCount = static_cast<uint32_t>(1),
                .descriptorType = m_Desc.Type,
                .pImageInfo = &descriptor
            };
            vkUpdateDescriptorSets(m_diCore->GetLogicalDevice(), 1, &writeDescSet, 0, nullptr);
        }

        void UpdateDescriptors(const std::vector<DescriptorHandle*>& handles)
        {
            std::vector<VkWriteDescriptorSet> writeDescs(handles.size());
            std::vector<VkDescriptorImageInfo> imageInfos(handles.size());
            for (uint32_t i = 0; i < writeDescs.size(); i++)
            {
                const DescriptorHandle& handle = *handles[i];
                imageInfos[i] = { .sampler = handle.Sampler->Get(), .imageView = handle.View->Get(), .imageLayout = handle.Layout};
                writeDescs[i] = { // used to update a descriptor in a descriptor set
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = m_Set,
                    .dstBinding = 0,
                    .dstArrayElement = handle.Index,
                    .descriptorCount = static_cast<uint32_t>(1),
                    .descriptorType = m_Desc.Type,
                    .pImageInfo = &imageInfos[i]
                };
            }

            vkUpdateDescriptorSets(m_diCore->GetLogicalDevice(), writeDescs.size(), writeDescs.data(), 0, nullptr);
        }

        // todo Impl DescriptorHandle creation function that combines ::UpdateDescriptor() with an index freelist

        VkDescriptorSet Get() const { return m_Set; }
        VkDescriptorSetLayout GetLayout() const { return m_Layout; }

    private:
        DescriptorSet(Core& core, DescriptorPool& pool, VkDescriptorSet set, VkDescriptorSetLayout layout, const DescriptorSet::Desc& desc)
        {
            m_diCore = &core;
            m_diPool = &pool;
            m_Set = set;
            m_Layout = layout;
            m_Desc = desc;
        }

        VkDescriptorSet m_Set;
        VkDescriptorSetLayout m_Layout;
        Desc m_Desc;

        RenderAPI::Core* m_diCore = nullptr;
        RenderAPI::DescriptorPool* m_diPool = nullptr;
    };

    // todo Make it support other type of descriptors than VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    class DescriptorPool
    {
    public:
        struct Desc
        {
            uint32_t MaxSets;
            std::vector<DescriptorSet::Desc> Sets;
        };

        DescriptorPool() = default;
        DescriptorPool(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        DescriptorSet CreateDescriptorSet(const DescriptorSet::Desc& desc)
        {
            VkDescriptorBindingFlags descVariableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
            VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = 1,
                .pBindingFlags = &descVariableFlag
            };
            VkDescriptorSetLayoutBinding descLayoutBindingTex{
                .descriptorType = desc.Type,
                .descriptorCount = desc.DescriptorCount,
                .stageFlags = desc.ShaderStages/*VK_SHADER_STAGE_FRAGMENT_BIT*/
            };
            VkDescriptorSetLayoutCreateInfo descLayoutTexCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &descBindingFlags,
                .bindingCount = descBindingFlags.bindingCount,
                .pBindings = &descLayoutBindingTex
            };

            VkDescriptorSetLayout layout;
            chkSucceed(vkCreateDescriptorSetLayout(m_diCore->GetLogicalDevice(), &descLayoutTexCI, nullptr, &layout));

            uint32_t variableDescCount{ descBindingFlags.bindingCount };
            VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
                .descriptorSetCount = descBindingFlags.bindingCount,
                .pDescriptorCounts = &variableDescCount
            };
            VkDescriptorSetAllocateInfo descSetAlloc{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = &variableDescCountAI,
                .descriptorPool = m_DescriptorPool,
                .descriptorSetCount = descBindingFlags.bindingCount,
                .pSetLayouts = &layout
            };

            VkDescriptorSet set;
            chkSucceed(vkAllocateDescriptorSets(m_diCore->GetLogicalDevice(), &descSetAlloc, &set));

            return DescriptorSet(*m_diCore, *this, set, layout, desc);
        }

        void Init(Core& core, const Desc& desc)
        {
            std::vector<VkDescriptorPoolSize> sets(desc.Sets.size());
            for (uint32_t i = 0; i < sets.size(); i++)
            {
                sets[i].type = desc.Sets[i].Type;
                sets[i].descriptorCount = desc.Sets[i].DescriptorCount;
            }

            VkDescriptorPoolCreateInfo descPoolCI{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .maxSets = desc.MaxSets,
                .poolSizeCount = static_cast<uint32_t>(sets.size()),
                .pPoolSizes = sets.data()
            };
            chkSucceed(vkCreateDescriptorPool(core.GetLogicalDevice(), &descPoolCI, nullptr, &m_DescriptorPool));

            m_diCore = &core;
            m_Desc = desc;
        }

        VkDescriptorPool Get() const { return m_DescriptorPool; }

    private:
        VkDescriptorPool m_DescriptorPool;
        Core* m_diCore = nullptr;
        Desc m_Desc;
    };
}