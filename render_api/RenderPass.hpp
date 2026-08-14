#pragma once
#include "Core.hpp"
#include "Compiler.hpp"
#include "CommandPool.hpp"
#include "Descriptor.hpp"
#include <functional>

namespace aZero::Pipeline
{
    class RenderPass;

    enum class ERenderPassType { Graphics, Compute };
    
    using RenderCallback = std::function<void(RenderAPI::CommandBuffer&, aZero::Pipeline::RenderPass&)>;

    /*
        NOTE!
        Currently only supports one binding per set.
    */
    class RenderPass : public DeleteCopy
    {
    public:

        struct Reflection
        {
            std::unordered_map<std::string, Compiler::PushConstantRangeReflection> PushConstants;
        };

        RenderAPI::Binding GetPushConstantBinding(const std::string& block, const std::string& member) const
        {
            RenderAPI::Binding binding;
            if (const auto iter = m_Reflection.PushConstants.find(block); iter != m_Reflection.PushConstants.end())
            {
                if (const auto iter2 = iter->second.Members.find(member); iter2 != iter->second.Members.end())
                {
                    binding = RenderAPI::Binding(iter2->second, m_Layout);
                }
            }
            return binding;
        }

        RenderAPI::Binding GetPushConstantBlock(const std::string& block) const
        {
            RenderAPI::Binding binding;
            if (const auto iter = m_Reflection.PushConstants.find(block); iter != m_Reflection.PushConstants.end())
            {
                binding = RenderAPI::Binding(iter->second.Range, m_Layout);
            }
            return binding;
        }

        RenderPass() = default;
        ~RenderPass()
        {
            if (m_diCore)
            {
                vkDestroyPipelineLayout(m_diCore->GetLogicalDevice(), m_Layout, nullptr);
                vkDestroyPipeline(m_diCore->GetLogicalDevice(), m_Pipeline, nullptr);
            }
        }

        RenderPass(RenderPass&& other) noexcept
        {
            *this = std::move(other);
        }

        RenderPass& operator=(RenderPass&& other) noexcept
        {
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_Type, other.m_Type);
            std::swap(m_NumColorTargets, other.m_NumColorTargets);
            std::swap(m_HasDepthStencilTarget, other.m_HasDepthStencilTarget);
            std::swap(m_Reflection, other.m_Reflection);
            std::swap(m_Layout, other.m_Layout);
            std::swap(m_Pipeline, other.m_Pipeline);
            return *this;
        }

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

            RenderCallback Callback;
        };

        bool Render(RenderAPI::CommandBuffer& commandBuffer, const RenderDesc& renderDesc)
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
            if (!m_HasDepthStencilTarget && renderDesc.DepthTarget.View)
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

            renderDesc.Callback(commandBuffer, *this);

            commandBuffer.EndRender();

            return true;
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
            Compiler::ShaderProgram* Program = nullptr;
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
        };

        using MeshShaderPassDesc = Desc;

        bool Compile(const MeshShaderPassDesc& desc)
        {
            return this->CompilePipeline(desc, {}, {});
        }

        bool Compile(const VertexPassDesc& desc)
        {
            std::vector<VkVertexInputAttributeDescription> vertexAttributes;

            std::vector<VkVertexInputBindingDescription> vertexBindings(desc.VertexBindings.size());
            for (uint32_t i = 0; i < vertexBindings.size(); i++)
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

            VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{ // defines how the primitives are assembled
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = desc.RasterDesc.PrimitiveMode == VkPolygonMode::VK_POLYGON_MODE_FILL ?
                    VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : desc.RasterDesc.PrimitiveMode == VkPolygonMode::VK_POLYGON_MODE_LINE ?
                    VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_POINT_LIST
            };

            return this->CompilePipeline(desc, vertexInputState, inputAssemblyState);
        }

        VkPipeline GetPipeline() const { return m_Pipeline; }
        VkPipelineLayout GetPipelineLayout() const { return m_Layout; }
        const Reflection& GetReflection() const { return m_Reflection; }

    private:
        void PopulateReflection(const Compiler::ShaderProgram::Reflection& reflection)
        {
            m_Reflection.PushConstants = reflection.PushConstants;
        }

        template<typename T>
        bool CompilePipeline(const T& desc, std::optional<VkPipelineVertexInputStateCreateInfo> vertexInputState, std::optional<VkPipelineInputAssemblyStateCreateInfo> inputAssemblyState)
        {
            if (!m_diCore) {
                std::cout << "No vulkan core available.\n";
                return false;
            }

            if (!desc.Program) {
                std::cout << "No program input.\n";
                return false;
            }

            const Compiler::ShaderProgram::Reflection& reflection = desc.Program->GetReflection();
            std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

            for (const auto& [stageName, moduleInfo] : reflection.Modules)
            {
                VkPipelineShaderStageCreateInfo stageCI{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = moduleInfo.Stage,
                    .module = moduleInfo.VkModule,
                    .pName = moduleInfo.Name.c_str()
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

            std::vector<VkDescriptorSetLayout> layouts;
            for (const auto& [setIndex, set] : reflection.DescriptorSets)
            {
                layouts.push_back(set.Layout);
            }

            std::vector<VkPushConstantRange> pcRanges;
            for (const auto& [offset, constant] : reflection.PushConstants)
            {
                pcRanges.push_back(constant.Range);
            }

            VkPipelineLayoutCreateInfo pipelineLayoutCI{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .setLayoutCount = (uint32_t)layouts.size(),
                .pSetLayouts = layouts.data(),
                .pushConstantRangeCount = (uint32_t)pcRanges.size(),
                .pPushConstantRanges = pcRanges.data()
            };

            if (!aZero::RenderAPI::chkSucceed(vkCreatePipelineLayout(m_diCore->GetLogicalDevice(), &pipelineLayoutCI, nullptr, &m_Layout)))
            {
                return false;
            }

            VkGraphicsPipelineCreateInfo pipelineCI{
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext = &renderingCI, // pass the VkPipelineRenderingCreateInfo as pNext since theres no dedicated member in the VkGraphicsPipelineCreateInfo struct (since dynamic rendering was added later)
                .stageCount = (uint32_t)shaderStages.size(),
                .pStages = shaderStages.data(),
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizationState,
                .pMultisampleState = &multisampleState,
                .pDepthStencilState = &depthStencilState,
                .pColorBlendState = &colorBlendState,
                .pDynamicState = &dynamicState,
                .layout = m_Layout
            };

            if (vertexInputState.has_value()) {
                pipelineCI.pVertexInputState = &vertexInputState.value();
            }
            if (inputAssemblyState.has_value()) {
                pipelineCI.pInputAssemblyState = &inputAssemblyState.value();
            }

            // VkPipelineCache can be used here to cache the result for future re-use. This can be saved and then loaded again between app runs.
            if (!aZero::RenderAPI::chkSucceed(vkCreateGraphicsPipelines(m_diCore->GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_Pipeline)))
            {
                return false;
            }

            this->PopulateReflection(desc.Program->GetReflection());
            m_Type = ERenderPassType::Graphics;
            return true;
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