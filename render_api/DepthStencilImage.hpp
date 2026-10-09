#pragma once
#include "Image.hpp"

namespace aZero::RenderAPI
{
    class DepthStencilImage : public DeleteCopy
    {
    public:
        struct Desc
        {
            uint32_t Width = 0u, Height = 0u;
        };

        DepthStencilImage() = default;
        DepthStencilImage(DepthStencilImage&& other) noexcept
        {
            *this = std::move(other);
        }
        DepthStencilImage& operator=(DepthStencilImage&& other) noexcept
        {
            std::swap(m_Image, other.m_Image);
            return *this;
        }

        DepthStencilImage(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        bool Init(Core& core, const Desc& desc)
        {
            std::vector<VkFormat> depthFormats{ VK_FORMAT_D24_UNORM_S8_UINT };
            VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
            for (VkFormat& format : depthFormats)
            {
                VkFormatProperties2 formatProps{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
                vkGetPhysicalDeviceFormatProperties2(core.GetPhysicalDevice(), format, &formatProps);
                if (formatProps.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                {
                    depthFormat = format;
                    break;
                }
            }

            if (depthFormat == VK_FORMAT_UNDEFINED)
            {
                throw std::runtime_error("Unsupported depth format.");
            }

            RenderAPI::Image::Desc imageDesc{
                .Format = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT,
                .Type = VkImageType::VK_IMAGE_TYPE_2D,
                .Usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .Width = desc.Width, .Height = desc.Height, .Depth = 1u, .MipCount = 1u, .ArrayLayers = 1u
            };

            return m_Image.Init(core, imageDesc);
        }

        RenderAPI::Image& Get() { return m_Image; }

    private:
        RenderAPI::Image m_Image;
    };
}