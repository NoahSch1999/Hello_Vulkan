#pragma once
#include "Core.hpp"
#include "Semaphore.hpp"
#include "CommandPool.hpp"

namespace aZero::RenderAPI
{
    static inline void chkSwapchain(VkResult result) {
        if (result < VK_SUCCESS) {
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                //updateSwapchain = true;
                return;
            }
            std::cerr << "Vulkan call returned an error (" << result << ")\n";
            exit(result);
        }
    }

	class Swapchain : public DeleteCopy
	{
	public:
        struct Desc
        {
            VkSurfaceKHR Surface;
            EImageFormat Format;
            uint32_t FramesInFlight;
            uint32_t Width, Height;
        };

        Swapchain() = default;
        ~Swapchain()
        {
            if (m_diCore)
            {
                vkDestroySwapchainKHR(m_diCore->GetLogicalDevice(), m_Swapchain, nullptr);
                vkDestroySurfaceKHR(m_diCore->GetInstance(), m_Surface, nullptr);
                for (auto& bbImageView : m_BackBufferViews)
                {
                    vkDestroyImageView(m_diCore->GetLogicalDevice(), bbImageView, nullptr);
                }
            }
        }

        Swapchain(Swapchain&& other) noexcept
        {
            *this = std::move(other);
        }

        Swapchain& operator=(Swapchain&& other) noexcept
        {
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_CurrentFrameIndex, other.m_CurrentFrameIndex);
            std::swap(m_CurrentBackBuffer, other.m_CurrentBackBuffer);
            std::swap(m_Surface, other.m_Surface);
            std::swap(m_Swapchain, other.m_Swapchain);
            std::swap(m_CreateInfo, other.m_CreateInfo);
            std::swap(m_BackBuffers, other.m_BackBuffers);
            std::swap(m_BackBufferViews, other.m_BackBufferViews);
            std::swap(m_ImageReadySemaphores, other.m_ImageReadySemaphores);
            std::swap(m_RenderCompleteSemaphores, other.m_RenderCompleteSemaphores);
            return *this;
        }

        Swapchain(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

		void Init(Core& core, const Desc& desc)
		{
            // get surface properties
            VkSurfaceCapabilitiesKHR surfaceCaps{};
            chkSucceed(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(core.GetPhysicalDevice(), desc.Surface, &surfaceCaps));

            // get surface extents. On "Wayland" 0xFFFFFFFF means that we need to choose the dimensions
            VkExtent2D swapchainExtent{ surfaceCaps.currentExtent };
            if (surfaceCaps.currentExtent.width == 0xFFFFFFFF) {
                swapchainExtent = { .width = desc.Width, .height = desc.Height };
            }

            VkExtent2D extents{ .width = desc.Width, .height = desc.Height };
            VkSwapchainCreateInfoKHR swapchainCI{
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = desc.Surface,
                .minImageCount = desc.FramesInFlight,
                .imageFormat = RenderAPI::ToVK_ImageFormat(desc.Format),
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
            m_ImageReadySemaphores.resize(imageCount);
            m_RenderCompleteSemaphores.resize(imageCount);
            for (auto i = 0; i < imageCount; i++) {
                VkImageViewCreateInfo viewCI{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = m_BackBuffers[i], .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = RenderAPI::ToVK_ImageFormat(desc.Format), .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 } };
                chkSucceed(vkCreateImageView(core.GetLogicalDevice(), &viewCI, nullptr, &m_BackBufferViews[i]));
                m_ImageReadySemaphores[i] = RenderAPI::Semaphore(core, RenderAPI::ESemaphoreType::Binary);
                m_RenderCompleteSemaphores[i] = RenderAPI::Semaphore(core, RenderAPI::ESemaphoreType::Binary);
            }

            m_Surface = desc.Surface;
            m_CreateInfo = swapchainCI;
            m_diCore = &core;
		}

        void Resize(uint32_t width, uint32_t height)
        {
            m_diCore->FlushGPU();

            VkSurfaceCapabilitiesKHR caps{};
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                m_diCore->GetPhysicalDevice(),
                m_Surface,
                &caps
            );

            if (caps.currentExtent.width != UINT32_MAX) // When this isn't UINT32_MAX then that means that the system has chosen the extents itself. Ex: When resizing the window.
            {
                m_CreateInfo.imageExtent = caps.currentExtent;
            }
            else
            {
                m_CreateInfo.imageExtent.width =
                    std::clamp(width,
                        caps.minImageExtent.width,
                        caps.maxImageExtent.width);

                m_CreateInfo.imageExtent.height =
                    std::clamp(height,
                        caps.minImageExtent.height,
                        caps.maxImageExtent.height);
            }

            m_CreateInfo.oldSwapchain = m_Swapchain;
            m_CreateInfo.imageExtent = { .width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height) };
            chkSucceed(vkCreateSwapchainKHR(m_diCore->GetLogicalDevice(), &m_CreateInfo, nullptr, &m_Swapchain));
            for (auto i = 0; i < m_BackBuffers.size(); i++) {
                vkDestroyImageView(m_diCore->GetLogicalDevice(), m_BackBufferViews[i], nullptr);
            }

            uint32_t newBackBufferCount;
            chkSucceed(vkGetSwapchainImagesKHR(m_diCore->GetLogicalDevice(), m_Swapchain, &newBackBufferCount, nullptr));

            m_BackBuffers.resize(newBackBufferCount);

            chkSucceed(vkGetSwapchainImagesKHR(m_diCore->GetLogicalDevice(), m_Swapchain, &newBackBufferCount, m_BackBuffers.data()));

            m_BackBufferViews.resize(newBackBufferCount);
            for (auto i = 0; i < newBackBufferCount; i++) {
                VkImageViewCreateInfo viewCI{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = m_BackBuffers[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = m_CreateInfo.imageFormat,
                    .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
                };
                chkSucceed(vkCreateImageView(m_diCore->GetLogicalDevice(), &viewCI, nullptr, &m_BackBufferViews[i]));
            }
            for (auto& semaphore : m_RenderCompleteSemaphores) {
                vkDestroySemaphore(m_diCore->GetLogicalDevice(), semaphore.Get(), nullptr);
            }
            m_RenderCompleteSemaphores.resize(m_BackBuffers.size());
            for (auto& semaphore : m_RenderCompleteSemaphores) {
                semaphore = RenderAPI::Semaphore(*m_diCore, RenderAPI::ESemaphoreType::Binary);
            }
            vkDestroySwapchainKHR(m_diCore->GetLogicalDevice(), m_CreateInfo.oldSwapchain, nullptr);
        }

        struct PresentDesc
        {
            RenderAPI::CommandQueue* Queue;
            std::vector<RenderAPI::Semaphore*> WaitOnSemaphores;
        };

        void NextFrame(uint32_t frameIndex)
        {
            m_CurrentFrameIndex = frameIndex;
            chkSwapchain(vkAcquireNextImageKHR(m_diCore->GetLogicalDevice(), m_Swapchain, UINT64_MAX, m_ImageReadySemaphores.at(m_CurrentFrameIndex).Get(), VK_NULL_HANDLE, &m_CurrentBackBuffer)); // might not give us imageIndex that are in consecutive order, so we have a seperate index for the current swapchain and frameindex
        }

        void CompleteFrame(RenderAPI::CommandBuffer& commandBuffer) const
        {
            VkImageMemoryBarrier2 barrierPresent{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = this->GetCurrentBackBuffer(),
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
            };

            commandBuffer.Barrier({}, { barrierPresent }, {});
        }

        void Present(const PresentDesc& desc)
        {
            std::vector<VkSemaphore> waitOnSemaphores(desc.WaitOnSemaphores.size());
            for (uint32_t i = 0; i < waitOnSemaphores.size(); i++)
            {
                waitOnSemaphores[i] = desc.WaitOnSemaphores[i]->Get();
            }

            VkPresentInfoKHR presentInfo{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = (uint32_t)waitOnSemaphores.size(),
                .pWaitSemaphores = waitOnSemaphores.data(), // here we wait on the semaphore that is signalled when the command buffer has passed the OM
                .swapchainCount = 1u,
                .pSwapchains = &m_Swapchain,
                .pImageIndices = &m_CurrentBackBuffer
            };
            chkSwapchain(vkQueuePresentKHR(desc.Queue->Get(), &presentInfo));
        }

        uint32_t GetFramesInFlight() const { return m_BackBuffers.size(); }
        VkSwapchainKHR Get() const { return m_Swapchain; }
        VkImage GetCurrentBackBuffer() const { return m_BackBuffers.at(m_CurrentBackBuffer); }
        VkImageView GetCurrentBackBufferView() const { return m_BackBufferViews.at(m_CurrentBackBuffer); }
        RenderAPI::Semaphore& GetImageReadySemaphore() { return m_ImageReadySemaphores.at(m_CurrentFrameIndex); }
        RenderAPI::Semaphore& GetRenderCompletedSemaphore() { return m_RenderCompleteSemaphores.at(m_CurrentFrameIndex); }
        uint32_t GetCurrentBackBufferIndex() const { return m_CurrentBackBuffer; }
        VkSurfaceKHR GetSurface() const { return m_Surface; }
        const VkSwapchainCreateInfoKHR& GetLastCreateInfo() const { return m_CreateInfo; }
	private:
        uint32_t m_CurrentFrameIndex;
        uint32_t m_CurrentBackBuffer;
        VkSurfaceKHR m_Surface;
		VkSwapchainKHR m_Swapchain;
        VkSwapchainCreateInfoKHR m_CreateInfo;
        std::vector<VkImage> m_BackBuffers;
        std::vector<VkImageView> m_BackBufferViews;
        std::vector<RenderAPI::Semaphore> m_ImageReadySemaphores;
        std::vector<RenderAPI::Semaphore> m_RenderCompleteSemaphores;
        RenderAPI::Core* m_diCore = nullptr;
	};
}