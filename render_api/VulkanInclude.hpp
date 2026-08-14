#pragma once
#include <string>
#include <vector>
#include <utility>
#include <iostream>

// Vulkan and helpers include
#include "volk.h"
#include "vk_mem_alloc.h"
//

namespace aZero::RenderAPI
{
	inline uint32_t GetFormatStride(VkFormat format)
	{
		switch (format)
		{
		case VK_FORMAT_R32_UINT:
			return 4;

		case VK_FORMAT_R32_SFLOAT:
			return 4;

		case VK_FORMAT_R32G32_SFLOAT:
			return 8;

		case VK_FORMAT_R32G32B32_SFLOAT:
			return 12;

		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return 16;

		default:
			throw std::runtime_error("Unknown VkFormat size");
		}
	}
}