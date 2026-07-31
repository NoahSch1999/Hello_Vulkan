#pragma once
#include "Core.hpp"
#include "Compiler.hpp"

namespace aZero::Pipeline
{
    enum class ERenderPassType { Graphics, Compute };
    /*
        NOTE!
        Currently only supports one binding per set.
    */
    class RenderPass
    {
    public:
        struct Binding
        {
            std::size_t Binding;
            std::size_t Space;
            std::size_t Stride;
            std::size_t DescriptorCount;
            slang::TypeReflection::Kind Kind;
            slang::ParameterCategory Category;
            slang::BindingType Type;
        };

        struct Reflection
        {
            std::unordered_map<SlangStage, std::string> EntryPoints;
            std::unordered_map<std::string, Binding> Bindings;
        };

        RenderPass() = default;
        RenderPass(aZero::RenderAPI::Core& core)
            :m_diCore(&core) { }

        struct RenderDesc
        {
            struct ColorTarget
            {
                VkImageView View; // todo Change to RenderAPI::ImageView* once swapchain backbuffers are hidden behind one.
                VkClearValue ClearColor{ .color{ 0.0f, 0.0f, 0.2f, 1.0f } };
                VkAttachmentLoadOp LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                VkAttachmentStoreOp StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            };
            std::vector<ColorTarget> ColorTargets;

            struct DepthStencilTarget
            {
                RenderAPI::ImageView* View;
                VkClearValue ClearColor{ .depthStencil = {1.0f,  0} };
                VkAttachmentLoadOp LoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                VkAttachmentStoreOp StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            };

            DepthStencilTarget DepthTarget;

            uint32_t RenderWidth = 0u, RenderHeight = 0u;

            std::vector<RenderAPI::DescriptorSet*> DescriptorSets;
        };

        bool BeginRender(RenderAPI::CommandBuffer& commandBuffer, const RenderDesc& renderDesc)
        {
            if (m_Type != ERenderPassType::Graphics)
            {
                std::cout << "Can't begin a render pass that isn't a ERenderPassType::Graphics.\n";
                return false;
            }

            if (!renderDesc.DepthTarget.View && m_HasDepthStencilTarget)
            {
                std::cout << "RenderDesc has no depth stencil set despite the pipeline outputting to one.\n";
                return false;
            }

            if (renderDesc.ColorTargets.size() != m_NumColorTargets)
            {
                std::cout << "RenderDesc render target count isn't the same as the render targets output to by the pipeline.\n";
                return false;
            }

            if (renderDesc.RenderWidth == 0u || renderDesc.RenderHeight == 0u)
            {
                std::cout << "RenderDesc has either RenderWidth or RenderHeight set to 0.\n";
                return false;
            }

            VkRenderingInfo renderingInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea{.extent{.width = renderDesc.RenderWidth, .height = renderDesc.RenderHeight }},
                .layerCount = 1
            };

            std::vector<VkRenderingAttachmentInfo> colorAttachments;
            if (!renderDesc.ColorTargets.empty())
            {
                for (uint32_t i = 0; i < renderDesc.ColorTargets.size(); i++)
                {
                    colorAttachments.push_back({
                        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                        .imageView = renderDesc.ColorTargets[i].View,
                        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                        .loadOp = renderDesc.ColorTargets[i].LoadOp,
                        .storeOp = renderDesc.ColorTargets[i].StoreOp,
                        .clearValue{ renderDesc.ColorTargets[i].ClearColor }
                        });
                }
            }

            renderingInfo.colorAttachmentCount = (uint32_t)colorAttachments.size();
            renderingInfo.pColorAttachments = colorAttachments.data();

            VkRenderingAttachmentInfo depthAttachmentInfo;
            if (!m_HasDepthStencilTarget)
            {
                depthAttachmentInfo = { // specific to dynamic rendering
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = renderDesc.DepthTarget.View->Get(),
                    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                    .loadOp = renderDesc.DepthTarget.LoadOp,
                    .storeOp = renderDesc.DepthTarget.StoreOp,
                    .clearValue = renderDesc.DepthTarget.ClearColor
                };
                renderingInfo.pDepthAttachment = &depthAttachmentInfo;
            }

            commandBuffer.BeginRender(renderingInfo);

            vkCmdBindPipeline(commandBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

            std::vector<VkDescriptorSet> descSets;
            for (const auto& set : renderDesc.DescriptorSets)
            {
                descSets.push_back(set->Get());
            }
            vkCmdBindDescriptorSets(commandBuffer.Get(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_Layout, 0, (uint32_t)descSets.size(), descSets.data(), 0, nullptr);

            return true;
        }

        void EndRender(RenderAPI::CommandBuffer& commandBuffer)
        {
            commandBuffer.EndRender();
        }

        struct Desc
        {
            struct DepthStencil
            {
                VkFormat DepthStencilFormat = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
                VkCompareOp DepthOp = VK_COMPARE_OP_LESS_OR_EQUAL;
                bool ShouldWrite = true;
                bool ShouldTest = true;

                VkPipelineDepthStencilStateCreateInfo ToPipelineCI() const
                {
                    return  { // depthstencil settings
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                        .depthTestEnable = ShouldTest ? VK_TRUE : VK_FALSE,
                        .depthWriteEnable = ShouldWrite ? VK_TRUE : VK_FALSE,
                        .depthCompareOp = DepthOp
                    };
                }
            };

            struct Rasterizer
            {
                VkCullModeFlags CullMode = VkCullModeFlagBits::VK_CULL_MODE_BACK_BIT;
                VkPolygonMode PrimitiveMode = VkPolygonMode::VK_POLYGON_MODE_FILL;
                float LineWidth = 1.f;

                VkPipelineRasterizationStateCreateInfo ToPipelineCI() const
                {
                    return { // raster desc
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                        .polygonMode = PrimitiveMode,
                        .cullMode = CullMode,
                        .lineWidth = LineWidth
                    };
                }
            };

            std::vector<VkFormat> RenderTargetFormats;
            std::optional<DepthStencil> DepthDesc;
            Rasterizer RasterDesc;
            Compiler::ShaderModule* Module = nullptr;
            std::string PS_EntryPoint;
        };

        struct VertexPassDesc : public Desc
        {
            struct VertexBinding
            {
                uint32_t Stride;
                VkVertexInputRate InputRate;
                std::vector<VkFormat> AttributeFormats;
                //{ // input layout
                //    {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT },
                //    {.location = 1, .binding = 0, .format = VK_FORMAT_R32_UINT, .offset = sizeof(glm::vec3) },
                //    {.location = 2, .binding = 0, .format = VK_FORMAT_R32_UINT, .offset = sizeof(glm::vec3) + sizeof(uint32_t)},
                //};
            };
            std::vector<VertexBinding> VertexBindings;
            std::string VS_EntryPoint;
        };

        bool CompileVertexPass(const VertexPassDesc& desc)
        {
            if (!m_diCore) {
                std::cout << "No vulkan core available.\n";
                return false;
            }

            if (!desc.Module) {
                std::cout << "No module input.\n";
                return false;
            }

            this->LinkAndReflect(*desc.Module, desc);

            if (!m_Reflection.EntryPoints.contains(SlangStage::SLANG_STAGE_VERTEX))
            {
                std::cout << "No vertex shader entry found in the module.\n";
                return false;
            }

            if (desc.VertexBindings.empty())
            {
                std::cout << "No vertex buffer binding in the pass description.\n";
                return false;
            }

            std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

            for (const auto& [stageType, entryPoint] : m_Reflection.EntryPoints)
            {
                VkPipelineShaderStageCreateInfo stageCI{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = Compiler::SlangStageToVkStage(stageType),
                    .module = desc.Module->Get(),
                    .pName = entryPoint.c_str()
                };
                shaderStages.push_back(stageCI);
            }

            VkPipelineViewportStateCreateInfo viewportState{ // specify viewport and scizzorrect
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1
            };
            std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR }; // make them dynamic so we can change them without recompiling the pipelinestate
            VkPipelineDynamicStateCreateInfo dynamicState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = 2,
                .pDynamicStates = dynamicStates.data()
            };

            VkPipelineRasterizationStateCreateInfo rasterizationState = desc.RasterDesc.ToPipelineCI();

            // todo Impl support
            std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(desc.RenderTargetFormats.size());
            for (int i = 0; i < blendAttachments.size(); i++) {
                blendAttachments[i] = { .colorWriteMask = 0xF };
            }
            VkPipelineColorBlendStateCreateInfo colorBlendState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = (uint32_t)blendAttachments.size(),
                .pAttachments = blendAttachments.data()
            };
            VkPipelineMultisampleStateCreateInfo multisampleState{ // sample desc
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
            };
            //

            // -------------------------
            // VERTEX

            {
                auto slangModule = desc.Module->GetSlangModule();
                slang::ProgramLayout* layout = slangModule->getLayout();
                slang::VariableLayoutReflection* globals = layout->getGlobalParamsVarLayout();
                slang::TypeLayoutReflection* typeLayout = globals->getTypeLayout();
            }

            std::vector<VkVertexInputAttributeDescription> vertexAttributes;

            std::vector<VkVertexInputBindingDescription> vertexBindings(desc.VertexBindings.size());
            for(uint32_t i = 0; i < vertexBindings.size(); i++)
            {
                vertexBindings[i] = {
                    .binding = i,
                    .stride = desc.VertexBindings[i].Stride,
                    .inputRate = desc.VertexBindings[i].InputRate
                };

                uint32_t localAttributeOffset = 0;
                for (uint32_t j = 0; j < desc.VertexBindings[i].AttributeFormats.size(); j++)
                {
                    VkVertexInputAttributeDescription attribute{
                        .location = static_cast<uint32_t>(vertexAttributes.size()),
                        .binding = i,
                        .format = desc.VertexBindings[i].AttributeFormats[j],
                        .offset = localAttributeOffset
                    };

                    localAttributeOffset += aZero::RenderAPI::GetFormatStride(desc.VertexBindings[i].AttributeFormats[j]);
                    
                    vertexAttributes.push_back(attribute);
                }
            };

            VkPipelineVertexInputStateCreateInfo vertexInputState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindings.size()),
                .pVertexBindingDescriptions = vertexBindings.data(),
                .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
                .pVertexAttributeDescriptions = vertexAttributes.data(),
            };
            // -------------------------

            VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{ // defines how the primitives are assembled
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = desc.RasterDesc.PrimitiveMode == VkPolygonMode::VK_POLYGON_MODE_FILL ?
                    VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : desc.RasterDesc.PrimitiveMode == VkPolygonMode::VK_POLYGON_MODE_LINE ?
                    VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_POINT_LIST
            };

            VkPipelineRenderingCreateInfo renderingCI{ // this tells the rendering pipeline that we are using "dynamic rendering"
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = (uint32_t)desc.RenderTargetFormats.size(),
                .pColorAttachmentFormats = desc.RenderTargetFormats.data(),
            };

            m_NumColorTargets = desc.RenderTargetFormats.size();

            VkPipelineDepthStencilStateCreateInfo depthStencilState;
            if (desc.DepthDesc.has_value())
            {
                renderingCI.depthAttachmentFormat = desc.DepthDesc->DepthStencilFormat;
                depthStencilState = desc.DepthDesc->ToPipelineCI();
                m_HasDepthStencilTarget = true;
            }

            std::vector<VkPushConstantRange> pushConstants;
            std::vector<VkDescriptorSetLayout> descLayouts;

            for (const auto& [bindingName, bindingInfo] : m_Reflection.Bindings)
            {
                if (bindingInfo.Type == slang::BindingType::PushConstant)
                {
                    VkPushConstantRange pushConstant{
                        .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
                        .size = (uint32_t)bindingInfo.Stride
                    };
                    pushConstants.push_back(pushConstant);
                }
                else if (bindingInfo.Type == slang::BindingType::CombinedTextureSampler || bindingInfo.Type == slang::BindingType::Sampler || bindingInfo.Type == slang::BindingType::Texture)
                {

                    VkDescriptorBindingFlags descVariableFlag;
                    VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                    };

                    VkDescriptorSetLayoutBinding layoutBinding{
                        .binding = (uint32_t)bindingInfo.Binding,
                        .descriptorType = Compiler::SlangBindingTypeToVkDescriptorType(bindingInfo.Type),
                        .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
                    };

                    VkDescriptorSetLayoutCreateInfo descLayoutTexCI{
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                        .pBindings = &layoutBinding
                    };

                    if (bindingInfo.DescriptorCount == SLANG_UNBOUNDED_SIZE)
                    {
                        layoutBinding.descriptorCount = 1024; // todo Not hardcode this...
                        descVariableFlag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

                        descBindingFlags.bindingCount = 1u;
                        descBindingFlags.pBindingFlags = &descVariableFlag;

                        descLayoutTexCI.pNext = &descBindingFlags;
                        descLayoutTexCI.bindingCount = 1u;
                    }
                    else
                    {
                        layoutBinding.descriptorCount = uint32_t(bindingInfo.DescriptorCount);
                    }

                    VkDescriptorSetLayout layout;
                    aZero::RenderAPI::chkSucceed(vkCreateDescriptorSetLayout(m_diCore->GetLogicalDevice(), &descLayoutTexCI, nullptr, &layout));
                    descLayouts.push_back(layout);
                }
            }

            VkPipelineLayoutCreateInfo pipelineLayoutCI{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = (uint32_t)descLayouts.size(),
                .pSetLayouts = descLayouts.data(),
                .pushConstantRangeCount = (uint32_t)pushConstants.size(),
                .pPushConstantRanges = pushConstants.data()
            };

            aZero::RenderAPI::chkSucceed(vkCreatePipelineLayout(m_diCore->GetLogicalDevice(), &pipelineLayoutCI, nullptr, &m_Layout));


            VkGraphicsPipelineCreateInfo pipelineCI{
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = &renderingCI, // pass the VkPipelineRenderingCreateInfo as pNext since theres no dedicated member in the VkGraphicsPipelineCreateInfo struct (since dynamic rendering was added later)
                .stageCount = (uint32_t)shaderStages.size(),
                .pStages = shaderStages.data(),
                .pVertexInputState = &vertexInputState,
                .pInputAssemblyState = &inputAssemblyState,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizationState,
                .pMultisampleState = &multisampleState,
                .pDepthStencilState = &depthStencilState,
                .pColorBlendState = &colorBlendState,
                .pDynamicState = &dynamicState,
                .layout = m_Layout
            };

            // VkPipelineCache can be used here to cache the result for future re-use. This can be saved and then loaded again between app runs.
            if (!aZero::RenderAPI::chkSucceed(vkCreateGraphicsPipelines(m_diCore->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_Pipeline)))
            {
                return false;
            }

            m_Type = ERenderPassType::Graphics;

            return true;
        }

        VkPipeline GetPipeline() const { return m_Pipeline; }
        VkPipelineLayout GetPipelineLayout() const { return m_Layout; }

    private:
        bool LinkAndReflect(Compiler::ShaderModule& module, const VertexPassDesc& desc)
        {
            auto slangModule = module.GetSlangModule();

            Slang::ComPtr<slang::IEntryPoint> vsEntry;
            slangModule->findEntryPointByName(desc.VS_EntryPoint.c_str(), vsEntry.writeRef());
            if (!vsEntry.get())
            {
                printf("No vertex shader entry point found: %s", desc.VS_EntryPoint.c_str());
                return false;
            }
            m_Reflection.EntryPoints[SlangStage::SLANG_STAGE_VERTEX] = desc.VS_EntryPoint;

            std::vector<slang::IComponentType*> components;
            components.push_back(slangModule);
            components.push_back(vsEntry);

            if (!desc.PS_EntryPoint.empty())
            {
                Slang::ComPtr<slang::IEntryPoint> psEntry;
                slangModule->findEntryPointByName(desc.PS_EntryPoint.c_str(), psEntry.writeRef());
                if (!psEntry.get())
                {
                    printf("No pixel shader entry point found: %s", desc.PS_EntryPoint.c_str());
                    return false;
                }
                m_Reflection.EntryPoints[SlangStage::SLANG_STAGE_FRAGMENT] = desc.PS_EntryPoint;
                components.push_back(psEntry);
            }

            Slang::ComPtr<slang::IComponentType> program;
            Slang::ComPtr<slang::IBlob> diagnostics;
            if (SLANG_FAILED(module.GetSession()->createCompositeComponentType(
                components.data(),
                components.size(),
                program.writeRef(),
                diagnostics.writeRef()))) 
            {
                if (diagnostics)
                {
                    std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << std::endl;
                }
                return false;
            }

            // 4. Link
            Slang::ComPtr<slang::IComponentType> linkedProgram;
            if (SLANG_FAILED(program->link(linkedProgram.writeRef(), diagnostics.writeRef())))
            {
                if (diagnostics)
                {
                    std::cerr << static_cast<const char*>(diagnostics->getBufferPointer()) << std::endl;
                }
                return false;
            }

            this->Reflect(module, linkedProgram);

            return true;
        }

        void Reflect(Compiler::ShaderModule& module, Slang::ComPtr<slang::IComponentType> linkedProgram)
        {
            slang::ProgramLayout* layout = linkedProgram->getLayout();
            slang::VariableLayoutReflection* globals = layout->getGlobalParamsVarLayout();
            slang::TypeLayoutReflection* typeLayout = globals->getTypeLayout();

            std::unordered_map<std::string, Binding> bindings;

            for (int i = 0; i < typeLayout->getFieldCount(); i++)
            {
                slang::VariableLayoutReflection* field = typeLayout->getFieldByIndex(i);

                Binding reflectedField;
                reflectedField.Binding = field->getOffset(slang::ParameterCategory::DescriptorTableSlot);
                reflectedField.Space = field->getOffset(slang::ParameterCategory::RegisterSpace);
                reflectedField.Kind = field->getType()->getKind();
                reflectedField.Category = field->getCategory();
                reflectedField.Type = typeLayout->getBindingRangeType(i);
                reflectedField.DescriptorCount = typeLayout->getBindingRangeBindingCount(i);
                reflectedField.Stride = field->getTypeLayout()->getElementTypeLayout()->getSize(slang::ParameterCategory::Uniform);
                bindings[field->getName()] = reflectedField;
            }

            m_Reflection.Bindings = std::move(bindings);
        }

        ERenderPassType m_Type;
        uint32_t m_NumColorTargets = 0;
        bool m_HasDepthStencilTarget = false;
        Reflection m_Reflection;
        VkPipelineLayout m_Layout;
        VkPipeline m_Pipeline;
        aZero::RenderAPI::Core* m_diCore = nullptr;
    };
}