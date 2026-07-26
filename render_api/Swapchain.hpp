#pragma once
#include "Core.hpp"

namespace aZero::RenderAPI
{
	class Swapchain
	{
	public:
        struct Desc
        {
            VkSurfaceKHR Surface;
            VkFormat Format;
            uint32_t FramesInFlight;
            uint32_t Width, Height;
        };

        Swapchain() = default;
        Swapchain(const Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

		void Init(const Core& core, const Desc& desc)
		{
            // get surface properties
            VkSurfaceCapabilitiesKHR surfaceCaps{};
            chkSucceed(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(core.GetPhysicalDevice(), desc.Surface, &surfaceCaps));

            // get surface extents. On "Wayland" 0xFFFFFFFF means that we need to choose the dimensions
            VkExtent2D swapchainExtent{ surfaceCaps.currentExtent };
            if (surfaceCaps.currentExtent.width == 0xFFFFFFFF) {
                swapchainExtent = { .width = desc.Width, .height = desc.Height };
            }

            const VkFormat imageFormat{ VK_FORMAT_B8G8R8A8_SRGB }; // why b8g8r8 order?
            VkExtent2D extents{ .width = desc.Width, .height = desc.Height };
            VkSwapchainCreateInfoKHR swapchainCI{
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = desc.Surface,
                .minImageCount = desc.FramesInFlight,
                .imageFormat = imageFormat,
                .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                .imageExtent = extents,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR
            };

            chkSucceed(vkCreateSwapchainKHR(core.GetLogicalDevice(), &swapchainCI, nullptr, &m_Swapchain));

            uint32_t imageCount{ 0 };
            chkSucceed(vkGetSwapchainImagesKHR(core.GetLogicalDevice(), m_Swapchain, &imageCount, nullptr));

            m_BackBuffers.resize(imageCount);
            chkSucceed(vkGetSwapchainImagesKHR(core.GetLogicalDevice(), m_Swapchain, &imageCount, m_BackBuffers.data()));

            m_BackBufferViews.resize(imageCount);
            for (auto i = 0; i < imageCount; i++) {
                VkImageViewCreateInfo viewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = m_BackBuffers[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = imageFormat, .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } };
                chkSucceed(vkCreateImageView(core.GetLogicalDevice(), &viewCI, nullptr, &m_BackBufferViews[i]));
            }
		}

        void Resize(uint32_t width, uint32_t height)
        {
            // todo Impl
        }

        uint32_t GetFramesInFlight() const { return m_BackBuffers.size(); }
        VkSwapchainKHR Get() const { return m_Swapchain; }
        VkImage GetBackBuffer(uint32_t index) const { return m_BackBuffers[index]; }
        VkImageView GetBackBufferView(uint32_t index) const { return m_BackBufferViews[index]; }

	private:
		VkSwapchainKHR m_Swapchain;
        std::vector<VkImage> m_BackBuffers;
        std::vector<VkImageView> m_BackBufferViews;
	};
}