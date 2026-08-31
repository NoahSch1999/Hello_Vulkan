#pragma once
#include "Core.hpp"

namespace aZero::RenderAPI
{
    enum class ESamplerMipMapMode { Nearest, Linear };
    enum class ESamplerFilterMode { Nearest, Linear, Cubic_Ext, Cubic_img };

    class Sampler : public DeleteCopy
    {
    public:
        struct Desc
        {
            float MaxLod = 1.f;
            float MaxAnisotropy = 0.f;
            ESamplerMipMapMode MipMapMode = ESamplerMipMapMode::Linear;
            ESamplerFilterMode FilterMode = ESamplerFilterMode::Linear;
        };

        Sampler() = default;
        ~Sampler()
        {
            if (m_diCore)
            {
                vkDestroySampler(m_diCore->GetLogicalDevice(), m_Sampler, nullptr);
            }
        }

        Sampler(Sampler&& other) noexcept
        {
            *this = std::move(other);
        }

        Sampler& operator=(Sampler&& other) noexcept
        {
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_Sampler, other.m_Sampler);
            std::swap(m_Desc, other.m_Desc);
            return *this;
        }

        Sampler(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

        // todo: Impl fail-safe
        void Init(Core& core, const Desc& desc)
        {
            VkFilter filterMode =
                desc.FilterMode == ESamplerFilterMode::Nearest ? VK_FILTER_NEAREST :
                desc.FilterMode == ESamplerFilterMode::Linear ? VK_FILTER_LINEAR :
                desc.FilterMode == ESamplerFilterMode::Cubic_Ext ? VK_FILTER_CUBIC_EXT :
                desc.FilterMode == ESamplerFilterMode::Cubic_img ? VK_FILTER_CUBIC_IMG : VK_FILTER_NEAREST;

            VkSamplerCreateInfo samplerCI{
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = filterMode,
                .minFilter = filterMode,
                .mipmapMode = desc.MipMapMode == ESamplerMipMapMode::Linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .anisotropyEnable = desc.MaxAnisotropy != 0.f ? VK_TRUE : VK_FALSE,
                .maxAnisotropy = desc.MaxAnisotropy,
                .maxLod = desc.MaxLod,
            };

            chkSucceed(vkCreateSampler(core.GetLogicalDevice(), &samplerCI, nullptr, &m_Sampler));
            m_diCore = &core;
            m_Desc = desc;
        }

        VkSampler Get() const { return m_Sampler; }

    private:
        RenderAPI::Core* m_diCore = nullptr;
        VkSampler m_Sampler;
        Desc m_Desc;
    };
}