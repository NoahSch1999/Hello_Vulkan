#include "CommandPool.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "RenderPass.hpp"

bool aZero::RenderAPI::ExecuteIndirectLayout::Init(Core& core, const Desc& desc)
{
    uint32_t offset = 0;
    std::vector<VkIndirectCommandsLayoutTokenEXT> tokens;
    for (const auto& token : desc.Tokens)
    {
        VkIndirectCommandsLayoutTokenEXT tokenTemp{
            .sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT,
            .pNext = nullptr,
            .type = token.Type,
            .data = {},
            .offset = offset,
        };

        VkIndirectCommandsPushConstantTokenEXT pcToken;
        if (token.Type == VkIndirectCommandsTokenTypeEXT::VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT)
        {
            pcToken.updateRange = {
                .stageFlags = desc.RenderPass->GetReflection().ShaderStages,
                .offset = 0,
                .size = token.Stride
            };

            tokenTemp.data.pPushConstant = &pcToken;
        }

        tokens.push_back(tokenTemp);

        offset += token.Stride;
    }

    VkIndirectCommandsLayoutCreateInfoEXT indirectCommandsLayoutCI{
        .sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_EXT,
        .shaderStages = desc.RenderPass->GetReflection().ShaderStages,
        .indirectStride = offset,
        .pipelineLayout = desc.RenderPass->GetPipelineLayout(),
        .tokenCount = (uint32_t)tokens.size(),
        .pTokens = tokens.data(),
    };

    m_diCore = &core;
    m_Desc = desc;
    return aZero::RenderAPI::chkSucceed(vkCreateIndirectCommandsLayoutEXT(core.GetLogicalDevice(), &indirectCommandsLayoutCI, nullptr, &m_Layout));
}

void aZero::RenderAPI::CommandBuffer::SetVertexBuffer(const RenderAPI::Buffer& buffer, uint64_t offset) const
{
    VkDeviceSize offsetTemp{ offset };
    std::vector<VkBuffer> buffers{ buffer.Get() };
    vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, buffers.data(), &offsetTemp);
}

void aZero::RenderAPI::CommandBuffer::SetIndexBuffer(const RenderAPI::Buffer& buffer, uint64_t offset, VkIndexType indexType) const
{
    VkDeviceSize offsetTemp{ offset };
    vkCmdBindIndexBuffer(m_CommandBuffer, buffer.Get(), offsetTemp, indexType);
}

void aZero::RenderAPI::CommandBuffer::Barrier(const std::vector<VkMemoryBarrier2>& globalBarriers, const std::vector<VkImageMemoryBarrier2>& imageBarriers, const std::vector<VkBufferMemoryBarrier2>& bufferBarriers) const
{
    VkDependencyInfo barrierDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = (uint32_t)globalBarriers.size(),
        .pMemoryBarriers = globalBarriers.data(),
        .bufferMemoryBarrierCount = (uint32_t)bufferBarriers.size(),
        .pBufferMemoryBarriers = bufferBarriers.data(),
        .imageMemoryBarrierCount = (uint32_t)imageBarriers.size(),
        .pImageMemoryBarriers = imageBarriers.data()
    };
    vkCmdPipelineBarrier2(m_CommandBuffer, &barrierDependencyInfo);
}

void aZero::RenderAPI::CommandBuffer::Copy(const BufferToImageCopyDesc& copyDesc, const RenderAPI::Buffer& srcBuffer, const RenderAPI::Image& dstImage)
{
    std::vector<VkBufferImageCopy> copyRegionsTemp{};
    for (uint32_t j = 0; j < copyDesc.MipOffsets.size(); j++) {
        copyRegionsTemp.push_back({
            .bufferOffset = copyDesc.MipOffsets[j],
            .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = j, .layerCount = 1},
            .imageExtent{.width = copyDesc.BaseWidth >> j, .height = copyDesc.BaseHeight >> j, .depth = copyDesc.DepthOrArrayCount },
            });
    }
    vkCmdCopyBufferToImage(m_CommandBuffer, srcBuffer.Get(), dstImage.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copyRegionsTemp.size()), copyRegionsTemp.data());
}

bool aZero::RenderAPI::CommandBuffer::ExecuteIndirectDGC(const ExecuteIndirectDGCDesc& desc)
{
    if (
        !desc.RenderPass
        || !desc.Layout
        || !desc.IndirectCommandsBuffer
        || !desc.PreprocessBuffer
        || !desc.SequencesBuffer
        || !desc.IndirectCommandsBuffer->GetDeviceAddress().has_value()
        || !desc.PreprocessBuffer->GetDeviceAddress().has_value()
        || !desc.SequencesBuffer->GetDeviceAddress().has_value()
    )
    {
        return false;
    }

    VkGeneratedCommandsPipelineInfoEXT pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_PIPELINE_INFO_EXT,
        .pNext = nullptr,
        .pipeline = desc.RenderPass->GetPipeline(),
    };
    
    VkGeneratedCommandsInfoEXT generatedCmds{
        .sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_EXT,
        .pNext = &pipelineInfo,
        .shaderStages = desc.RenderPass->GetReflection().ShaderStages,
        .indirectExecutionSet = VK_NULL_HANDLE,
        .indirectCommandsLayout = desc.Layout->Get(),
        .indirectAddress = desc.IndirectCommandsBuffer->GetDeviceAddress().value(),
        .indirectAddressSize = desc.IndirectCommandsBuffer->GetDesc().Size,
        .preprocessAddress = desc.PreprocessBuffer->GetDeviceAddress().value(),
        .preprocessSize = desc.PreprocessBuffer->GetDesc().Size,
        .maxSequenceCount = desc.MaxSequences,
        .sequenceCountAddress = desc.SequencesBuffer->GetDeviceAddress().value(),
        .maxDrawCount = desc.MaxDraws,
    };

    vkCmdExecuteGeneratedCommandsEXT(m_CommandBuffer, VK_FALSE, &generatedCmds);

    return true;
}

bool aZero::RenderAPI::CommandBuffer::ExecuteIndirectMeshTasksCount(const ExecuteIndirectDesc& desc)
{
    if (
           !desc.IndirectArgumentBuffer
        || !desc.CountBuffer
        || desc.IndirectArgumentStride == 0
        || desc.MaxDraws == 0
        )
    {
        return false;
    }

    vkCmdDrawMeshTasksIndirectCountEXT(m_CommandBuffer, desc.IndirectArgumentBuffer->Get(), desc.IndirectArgumentBufferOffset, desc.CountBuffer->Get(), desc.CountBufferOffset, desc.MaxDraws, desc.IndirectArgumentStride);
    
    return true;
}