#pragma once
#include "VulkanInclude.hpp"

namespace aZero::RenderAPI
{
	inline bool chkSucceed(VkResult result)
	{
		if (result != VK_SUCCESS) {
			std::cerr << "Vulkan call returned an error (" << result << ")\n";
			return false;
		}

		return true;
	}

	inline VkMemoryRequirements2 GetGeneratedCommandsMemoryReq(VkDevice device, VkPipeline pipeline, VkIndirectCommandsLayoutEXT icLayout, uint32_t maxSequenceCount, uint32_t maxDrawCount)
	{
        VkGeneratedCommandsPipelineInfoEXT pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_PIPELINE_INFO_EXT,
        .pNext = nullptr,
        .pipeline = pipeline
        };

        VkGeneratedCommandsMemoryRequirementsInfoEXT memoryInfo{
            .sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_EXT,
            .pNext = &pipelineInfo,
            .indirectExecutionSet = VK_NULL_HANDLE,
            .indirectCommandsLayout = icLayout,
            .maxSequenceCount = maxSequenceCount,
            .maxDrawCount = maxDrawCount
        };

        VkMemoryRequirements2 req{
            .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2
        };

        vkGetGeneratedCommandsMemoryRequirementsEXT(
            device,
            &memoryInfo,
            &req);

        return req;
	}
}