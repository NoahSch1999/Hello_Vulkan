#pragma once
#include "slang.h"
#include "slang-com-ptr.h"
#include "Core.hpp"
#include "spirv_reflect.h"
#include <unordered_set>
#include <unordered_map>
#include <variant>
#include <filesystem>
#include <fstream>
#include <array>

namespace aZero::Compiler
{
    struct PushConstantRangeReflection
    {
        VkPushConstantRange Range;
        std::unordered_map<std::string, VkPushConstantRange> Members;
    };

    inline VkShaderStageFlagBits SlangStageToVkStage(SlangStage stage)
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

    inline VkDescriptorType SpirvBindingTypeToVkDescriptorType(SpvReflectDescriptorType type)
    {
        switch (type)
        {
        case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        {
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        {
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        {
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        {
            return VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER;
        }
        case SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
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
        friend class ShaderProgram;
        friend class CompilerSession;
    public:
        ShaderModule() = default;

    private:
        ShaderModule(Slang::ComPtr<slang::IModule> module, Slang::ComPtr<slang::ISession> session, RenderAPI::Core& core)
            :m_SlangModule(module), m_Session(session), m_diCore(&core) { }
       
        Slang::ComPtr<slang::IModule> m_SlangModule;
        Slang::ComPtr<slang::ISession> m_Session;
        RenderAPI::Core* m_diCore = nullptr;
    };

    class ShaderProgram : public DeleteCopy
    {
    public:

        struct DescriptorSetReflection
        {
            struct LayoutBinding
            {
                std::string Name;
                bool HasVariableBinding = false;
                VkDescriptorSetLayoutBinding VkLayout;
            };

            VkDescriptorSetLayout Layout;
            std::unordered_map<uint32_t, LayoutBinding> LayoutBindings;
        };

        struct ModuleReflection
        {
            std::string Name;
            VkShaderModule VkModule;
            VkShaderStageFlagBits Stage;
        };

        struct Reflection
        {
            std::unordered_map<std::string, ModuleReflection> Modules;
            std::unordered_map<std::string, PushConstantRangeReflection> PushConstants;
            std::unordered_map<uint32_t, DescriptorSetReflection> DescriptorSets;
        };

        struct VertexDesc
        {
            std::unordered_set<std::string> EntryPoints;
            std::vector<ShaderModule*> Modules;
        };

        struct MeshletDesc
        {
            std::unordered_set<std::string> EntryPoints;
            std::vector<ShaderModule*> Modules;
        };

        struct ComputeDesc
        {
            std::string EntryPointName;
            std::vector<ShaderModule*> Modules;
        };

        using Desc = std::variant<VertexDesc, MeshletDesc, ComputeDesc>;

        ShaderProgram() = default;
        ~ShaderProgram()
        {
            if (m_diCore)
            {
                for (auto& [name, module] : m_Reflection.Modules)
                {
                    vkDestroyShaderModule(m_diCore->GetLogicalDevice(), module.VkModule, nullptr);
                }

                for (auto& [index, set] : m_Reflection.DescriptorSets)
                {
                    vkDestroyDescriptorSetLayout(m_diCore->GetLogicalDevice(), set.Layout, nullptr);
                }
            }
        }

        ShaderProgram(ShaderProgram&& other) noexcept
        {
            *this = std::move(other);
        }

        ShaderProgram& operator=(ShaderProgram&& other) noexcept
        {
            std::swap(m_Program, other.m_Program);
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_Reflection, other.m_Reflection);
            return *this;
        }

        ShaderProgram(const Desc& desc)
        {
            std::visit([this](auto&& programDesc)
            {
                Build(programDesc);
            }, desc);
        }

        template<typename T>
        bool Build(const T& desc)
        {
            if (desc.Modules.empty())
            {
                printf("No modules input.\n");
                return false;
            }

            std::vector<slang::IComponentType*> programComponents;
            for (const ShaderModule* module : desc.Modules)
            {
                programComponents.push_back(module->m_SlangModule);
            }

            this->AddEntryPoints(desc, programComponents);

            Slang::ComPtr<slang::IComponentType> builtProgram = this->BuildProgram(desc.Modules, programComponents);
            if (!builtProgram.get())
            {
                return false;
            }

            m_Program = builtProgram;
            m_diCore = desc.Modules[0]->m_diCore;

            return true;
        }

        const Reflection& GetReflection() const { return m_Reflection; }

    private:
        Slang::ComPtr<slang::IComponentType> m_Program;
        Reflection m_Reflection;
        RenderAPI::Core* m_diCore = nullptr;

        void AddEntryPoints(const MeshletDesc& desc, std::vector<slang::IComponentType*>& programComponents)
        {
            this->FindAddEntryPoints(programComponents, desc.EntryPoints, desc.Modules);
        }

        void AddEntryPoints(const VertexDesc& desc, std::vector<slang::IComponentType*>& programComponents)
        {
            this->FindAddEntryPoints(programComponents, desc.EntryPoints, desc.Modules);
        }

        void AddEntryPoints(const ComputeDesc& desc, std::vector<slang::IComponentType*>& programComponents)
        {
            this->FindAddEntryPoints(programComponents, { desc.EntryPointName }, desc.Modules);
        }

        void FindAddEntryPoints(std::vector<slang::IComponentType*>& programComponents, const std::unordered_set<std::string>& entryPoints, const std::vector<ShaderModule*>& modules)
        {
            for (const auto& module : modules)
            {
                for (const auto& entryPoint : entryPoints)
                {
                    Slang::ComPtr<slang::IEntryPoint> entry;
                    module->m_SlangModule->findEntryPointByName(entryPoint.c_str(), entry.writeRef());
                    if (entry.get())
                    {
                        programComponents.push_back(entry);
                    }
                }
            }
        }

        Slang::ComPtr<slang::IComponentType> BuildProgram(const std::vector<ShaderModule*>& modules, const std::vector<slang::IComponentType*>& programComponents)
        {
            Slang::ComPtr<slang::IComponentType> compiledProgram;
            Slang::ComPtr<slang::IBlob> diagnostics;
            if (SLANG_FAILED(modules[0]->m_Session->createCompositeComponentType(
                programComponents.data(),
                programComponents.size(),
                compiledProgram.writeRef(),
                diagnostics.writeRef())))
            {
                if (diagnostics)
                {
                    std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << std::endl;
                }
                return nullptr;
            }

            Slang::ComPtr<slang::IComponentType> linkedProgram;
            if (SLANG_FAILED(compiledProgram->link(linkedProgram.writeRef(), diagnostics.writeRef())))
            {
                if (diagnostics)
                {
                    std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << std::endl;
                }
                return nullptr;
            }

            std::unordered_map<std::string, PushConstantRangeReflection> pcRefl;
            std::unordered_map<uint32_t, DescriptorSetReflection> dsRefl;
            std::unordered_map<std::string, ModuleReflection> modulesRefl;

            Slang::ComPtr<slang::IBlob> spirvCode;
            uint32_t basePushConstantBlockOffset = 0;
            for (uint32_t i = 0; i < linkedProgram->getLayout()->getEntryPointCount(); i++)
            {
                if (SLANG_FAILED(linkedProgram->getEntryPointCode(i, 0 /* spirv set during session creation */, spirvCode.writeRef(), diagnostics.writeRef())))
                {
                    if (diagnostics)
                    {
                        std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << std::endl;
                    }
                    return nullptr;
                }

                slang::EntryPointReflection* entryPoint = linkedProgram->getLayout()->getEntryPointByIndex(i);
                spv_reflect::ShaderModule sprvReflection(spirvCode->getBufferSize(), spirvCode->getBufferPointer());
                SpvReflectShaderModule reflModule = sprvReflection.GetShaderModule();

                for (uint32_t j = 0; j < reflModule.push_constant_block_count; j++)
                {
                    const SpvReflectBlockVariable& block = reflModule.push_constant_blocks[j];
                    if (!pcRefl.contains(std::string(block.name)))
                    {
                        PushConstantRangeReflection rangeRefl{
                            .Range {
                               .offset = basePushConstantBlockOffset,
                               .size = block.size,
                            }
                        };

                        for (uint32_t k = 0; k < block.member_count; k++)
                        {
                            VkPushConstantRange memberRange{
                                .stageFlags = (VkShaderStageFlags)SlangStageToVkStage(entryPoint->getStage()),
                                .offset = block.members[k].offset,
                                .size = block.members[k].size
                            };
                            rangeRefl.Members[std::string(block.members[k].name)] = memberRange;
                        }
                        basePushConstantBlockOffset += block.size;
                        basePushConstantBlockOffset = (basePushConstantBlockOffset + 15) & ~size_t(15); // Force 16-byte alignment across blocks
                        pcRefl[std::string(block.name)] = rangeRefl;
                    }

                    for (auto& [name, member] : pcRefl[std::string(block.name)].Members)
                    {
                        member.stageFlags |= SlangStageToVkStage(entryPoint->getStage());
                    }
                    pcRefl[std::string(block.name)].Range.stageFlags |= SlangStageToVkStage(entryPoint->getStage());
                }

                for (uint32_t j = 0; j < reflModule.descriptor_set_count; j++)
                {
                    const SpvReflectDescriptorSet& set = reflModule.descriptor_sets[j];

                    for (uint32_t k = 0; k < set.binding_count; k++)
                    {
                        const SpvReflectDescriptorBinding& binding = *set.bindings[k];
                        if (!dsRefl[set.set].LayoutBindings.contains(binding.binding))
                        {
                            VkDescriptorSetLayoutBinding vkBinding{
                                .binding = binding.binding,
                                .descriptorType = SpirvBindingTypeToVkDescriptorType(binding.descriptor_type),
                                .descriptorCount = binding.count
                            };

                            dsRefl[set.set].LayoutBindings[binding.binding].Name = binding.name;
                            dsRefl[set.set].LayoutBindings[binding.binding].VkLayout = vkBinding;
                        }

                        dsRefl[set.set].LayoutBindings[binding.binding].VkLayout.stageFlags |= SlangStageToVkStage(entryPoint->getStage());
                    }
                }

                VkShaderModuleCreateInfo shaderModuleCI{
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .codeSize = spirvCode->getBufferSize(),
                    .pCode = (uint32_t*)spirvCode->getBufferPointer()
                };

                VkShaderModule shaderModule;
                if(!aZero::RenderAPI::chkSucceed(vkCreateShaderModule(modules[0]->m_diCore->GetLogicalDevice(), &shaderModuleCI, nullptr, &shaderModule)))
                {
                    return nullptr;
                }

                VkShaderStageFlagBits stage = SlangStageToVkStage(entryPoint->getStage());
                
                modulesRefl[entryPoint->getName()] = {
                    .Name = std::string(reflModule.entry_point_name),
                    .VkModule = shaderModule,
                    .Stage = stage
                };
            }

            for (auto& [setIndex, reflectedSet] : dsRefl) 
            {
                bool hasVariableBinding = false;
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                std::vector<VkDescriptorBindingFlags> bindingFlags;
                for (auto& [bindingIndex, binding] : reflectedSet.LayoutBindings)
                {
                    VkDescriptorBindingFlags flags = 0;

                    if (binding.VkLayout.descriptorCount == 0)
                    {
                        binding.VkLayout.descriptorCount = modules[0]->m_diCore->GetVariadicDescriptorCount();
                        binding.HasVariableBinding = true;

                        flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                        hasVariableBinding = true;
                    }

                    bindings.push_back(binding.VkLayout);
                    bindingFlags.push_back(flags);
                }

                VkDescriptorSetLayoutCreateInfo descLayoutTexCI{
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                        .bindingCount = (uint32_t)bindings.size(),
                        .pBindings = bindings.data()
                };


                VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{
                       .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                       .bindingCount = (uint32_t)bindingFlags.size(),
                       .pBindingFlags = bindingFlags.data()
                };

                if (hasVariableBinding)
                {
                    descLayoutTexCI.pNext = &descBindingFlags;
                }

                /*
                    NOTE!
                    If a descriptor binding in the shader has [], aka VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, then it needs to be the last binding in that set.
                */
                if (!aZero::RenderAPI::chkSucceed(vkCreateDescriptorSetLayout(modules[0]->m_diCore->GetLogicalDevice(), &descLayoutTexCI, nullptr, &reflectedSet.Layout)))
                {
                    return nullptr;
                }
            }

            m_Reflection.Modules = std::move(modulesRefl);
            m_Reflection.PushConstants = std::move(pcRefl);
            m_Reflection.DescriptorSets = std::move(dsRefl);

            return linkedProgram;
        }

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
            //Slang::ComPtr<slang::IModule> module{ m_Session->loadModuleFromSource(path.stem().generic_string().c_str(), path.generic_string().c_str(), nullptr, diagnostics.writeRef())};

            Slang::ComPtr<slang::IModule> module{ m_Session->loadModule(path.generic_string().c_str(), diagnostics.writeRef()) };

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