#pragma once
#include "slang.h"
#include "slang/slang-com-ptr.h"
#include "Core.hpp"


/*
    auto slangOptions = std::to_array<slang::CompilerOptionEntry>({
    {
        slang::CompilerOptionName::EmitSpirvDirectly,
        {slang::CompilerOptionValueKind::Int, 1}
    },
    {
        slang::CompilerOptionName::DebugInformation,
        {slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_STANDARD}
    },
    {
        slang::CompilerOptionName::Optimization,
        {slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_NONE}
    }
    });
*/

namespace aZero::Compiler
{
    VkShaderStageFlagBits SlangStageToVkStage(SlangStage stage)
    {
        switch (stage)
        {
            case SlangStage::SLANG_STAGE_VERTEX:
            {
                return VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
            }
            case SlangStage::SLANG_STAGE_MESH:
            {
                return VkShaderStageFlagBits::VK_SHADER_STAGE_MESH_BIT_EXT;
            }
            case SlangStage::SLANG_STAGE_AMPLIFICATION:
            {
                return VkShaderStageFlagBits::VK_SHADER_STAGE_TASK_BIT_EXT;
            }
            case SlangStage::SLANG_STAGE_FRAGMENT:
            {
                return VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
            }
            case SlangStage::SLANG_STAGE_COMPUTE:
            {
                return VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;
            }
            default:
            {
                throw std::runtime_error("Stage type not supported.");
            }
        }
        return VkShaderStageFlagBits::VK_SHADER_STAGE_ALL;
    }

    VkDescriptorType SlangBindingTypeToVkDescriptorType(slang::BindingType type)
    {
        switch (type)
        {
            case slang::BindingType::CombinedTextureSampler:
            {
                return VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
            case slang::BindingType::RawBuffer:
            {
                return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            }
            case slang::BindingType::Texture:
            {
                return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            }
            case slang::BindingType::Sampler:
            {
                return VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER;
            }
            case slang::BindingType::ConstantBuffer:
            {
                return VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
            default:
            {
                throw std::runtime_error("Binding type not supported.");
            }
        }
        return VkDescriptorType::VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

    class ShaderModule
    {
        friend class CompilerSession;
    public:
       

        ShaderModule() = default;
        VkShaderModule Get() const { return m_ShaderModule; }
        slang::IModule* GetSlangModule() const { return m_SlangModule.get(); }
        slang::ISession* GetSession() const { return m_Session.get(); }

    private:
        ShaderModule(Slang::ComPtr<slang::IModule> module, Slang::ComPtr<slang::ISession> session, RenderAPI::Core& core)
            :m_SlangModule(module), m_Session(session), m_diCore(&core)
        {
            Slang::ComPtr<ISlangBlob> spirv;
            module->getTargetCode(0, spirv.writeRef());

            VkShaderModuleCreateInfo shaderModuleCI{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = spirv->getBufferSize(),
                .pCode = (uint32_t*)spirv->getBufferPointer()
            };

            aZero::RenderAPI::chkSucceed(vkCreateShaderModule(m_diCore->GetLogicalDevice(), &shaderModuleCI, nullptr, &m_ShaderModule));
        }
       
        Slang::ComPtr<slang::IModule> m_SlangModule;
        Slang::ComPtr<slang::ISession> m_Session;
        VkShaderModule m_ShaderModule;
        RenderAPI::Core* m_diCore = nullptr;
    };

    class CompilerSession
    {
        friend class Compiler;
    public:
        struct Desc
        {
            struct PreprocessorMacro
            {
                std::string Name;
                std::string Value;
            };
            std::vector<PreprocessorMacro> PreprocessorMacros;
            std::vector<std::filesystem::path> IncludePaths;
            std::vector<slang::CompilerOptionEntry> CompilerOptions;
        };

        CompilerSession() = default;

        std::optional<ShaderModule> LoadModule(const std::filesystem::path& path, bool outputReflectionJson = false)
        {
            if (!m_diCore || !std::filesystem::exists(path)) {
                return {};
            }

            Slang::ComPtr<slang::IBlob> diagnostics;
            Slang::ComPtr<slang::IModule> module{ m_Session->loadModuleFromSource(path.stem().generic_string().c_str(), path.generic_string().c_str(), nullptr, diagnostics.writeRef())};

            if (diagnostics)
            {
                std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << std::endl;
            }

            if (!module)
            {
                std::cerr << "Failed to load Slang module: " << path << std::endl;
                return {};
            }

            if (outputReflectionJson)
            {
                slang::ProgramLayout* layout = module->getLayout();
                Slang::ComPtr<slang::IBlob> diagnostics;
                layout->toJson(diagnostics.writeRef());

                std::filesystem::path jsonPath(path);
                jsonPath.replace_extension(".json");
                std::ofstream file(jsonPath, std::ios::trunc);
                file.write((char*)diagnostics->getBufferPointer(), diagnostics->getBufferSize());
                file.close();
            }

            return ShaderModule(module, m_Session, *m_diCore);
        }

    private:
        CompilerSession(Slang::ComPtr<slang::ISession> session, RenderAPI::Core& core)
            :m_Session(session), m_diCore(&core) {}

        Slang::ComPtr<slang::ISession> m_Session;
        RenderAPI::Core* m_diCore = nullptr;
    };

    class Compiler
    {
    public:
        Compiler() = default;

        Compiler(RenderAPI::Core& core)
        {
            slang::createGlobalSession(m_GlobalSession.writeRef());
            m_diCore = &core;
        }

        std::optional<CompilerSession> CreateSession(const CompilerSession::Desc& desc)
        {
            if (!m_diCore) {
                return {};
            }

            auto slangTargets{ std::to_array<slang::TargetDesc>({ {
                .format{SlangCompileTarget::SLANG_SPIRV},
                .profile{m_GlobalSession->findProfile("spirv_1_4")}}
            }) };

            std::vector<const char*> pathPtrs;
            pathPtrs.reserve(desc.IncludePaths.size());

            for (const auto& path : desc.IncludePaths)
            {
                pathPtrs.push_back(path.generic_string().c_str());
            }

            std::vector<slang::PreprocessorMacroDesc> preMacros;
            preMacros.reserve(desc.PreprocessorMacros.size());
            for (const auto& macro : desc.PreprocessorMacros)
            {
                preMacros.push_back({ macro.Name.c_str(), macro.Value.c_str() });
            }

            slang::SessionDesc slangSessionDesc{
                .targets{slangTargets.data()},
                .targetCount{SlangInt(slangTargets.size())},
                .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                .searchPaths = pathPtrs.data(),
                .searchPathCount = static_cast<SlangInt>(pathPtrs.size()),
                .preprocessorMacros = preMacros.data(),
                .preprocessorMacroCount = static_cast<SlangInt>(preMacros.size()),
                .compilerOptionEntries{desc.CompilerOptions.data()},
                .compilerOptionEntryCount{uint32_t(desc.CompilerOptions.size())}
            };

            Slang::ComPtr<slang::ISession> slangSession;
            if (SLANG_FAILED(m_GlobalSession->createSession(slangSessionDesc, slangSession.writeRef())))
            {
                return {};
            }

            return CompilerSession(slangSession, *m_diCore);
        }

    private:
        Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
        RenderAPI::Core* m_diCore = nullptr;
    };
}