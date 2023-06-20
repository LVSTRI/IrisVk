#include <iris/gfx/device.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/render_pass.hpp>
#include <iris/gfx/framebuffer.hpp>
#include <iris/gfx/pipeline.hpp>

#include <iris/core/utilities.hpp>

#include <shaderc/shaderc.hpp>

#include <spirv_glsl.hpp>
#include <spirv.hpp>

#include <volk.h>

#include <unordered_map>
#include <algorithm>
#include <utility>
#include <numeric>
#include <fstream>

namespace ir {
    namespace spvc = spirv_cross;
    namespace shc = shaderc;

    IR_NODISCARD static auto whole_file(const fs::path& path, bool is_binary = false) noexcept -> std::vector<uint8> {
        IR_PROFILE_SCOPED();
        // read the whole file into a vec<uint8>
        auto file = std::ifstream(path, std::ios::ate | (is_binary ? std::ios::binary : 0));
        auto size = file.tellg();
        auto data = std::vector<uint8>(size);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(data.data()), size);
        return data;
    }

    class shader_includer_t : public shc::CompileOptions::IncluderInterface {
    public:
        using self = shader_includer_t;
        using super = shc::CompileOptions::IncluderInterface;

        shader_includer_t(fs::path root) noexcept : _root(std::move(root)) {}

        ~shader_includer_t() noexcept override = default;

        auto GetInclude(const char* requested, shaderc_include_type, const char*, size_t) noexcept -> shaderc_include_result* override {
            IR_PROFILE_SCOPED();
            auto path = _root / requested;
            auto file = whole_file(path);
            auto content = std::string(reinterpret_cast<const char*>(file.data()), file.size());
            return new _include_result_t(std::move(content), path.filename().generic_string());
        }

        auto ReleaseInclude(shaderc_include_result* data) noexcept -> void override {
            IR_PROFILE_SCOPED();
            delete static_cast<_include_result_t*>(data);
        }

    private:
        class _include_result_t : public shaderc_include_result {
        public:
            using self = _include_result_t;
            using super = shaderc_include_result;

            _include_result_t(std::string content, std::string filename) noexcept
                : super(),
                  _content(std::move(content)),
                  _filename(std::move(filename)) {
                IR_PROFILE_SCOPED();
                super::content = _content.c_str();
                super::content_length = _content.size();
                super::source_name = _filename.c_str();
                super::source_name_length = _filename.size();
                super::user_data = nullptr;
            }

            ~_include_result_t() noexcept = default;

        private:
            std::string _content;
            std::string _filename;
        };

        fs::path _root;
    };
    
    IR_NODISCARD static auto compile_shader(
        const fs::path& path,
        shaderc_shader_kind kind,
        spdlog::logger& logger
    ) noexcept -> std::vector<uint32> {
        IR_PROFILE_SCOPED();
        const auto file = whole_file(path);
        auto compiler = shc::Compiler();
        auto options = shc::CompileOptions();
        auto include_path = path.parent_path();
        while (include_path.filename().generic_string() != "shaders") {
            include_path = include_path.parent_path();
        }
        /*options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        options.SetSourceLanguage(shaderc_source_language_glsl);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        options.SetTargetSpirv(shaderc_spirv_version_1_6);
        options.SetIncluder(std::make_unique<shader_includer_t>(std::move(include_path)));*/
        auto spirv = compiler.CompileGlslToSpv(reinterpret_cast<const char*>(file.data()), file.size(), kind, path.string().c_str(), options);
        if (spirv.GetCompilationStatus() != shaderc_compilation_status_success) {
            logger.error("shader compile failed:\n\"{}\"", spirv.GetErrorMessage());
            logger.flush();
            return {};
        }
        return { spirv.cbegin(), spirv.cend() };
    }

    pipeline_t::pipeline_t(const graphics_pipeline_create_info_t& info) noexcept : _info(info) {
        IR_PROFILE_SCOPED();
    }

    pipeline_t::~pipeline_t() noexcept {
        IR_PROFILE_SCOPED();
        const auto& device = _framebuffer.as_const_ref().render_pass().device();
        vkDestroyPipeline(device.handle(), _handle, nullptr);
        // TODO: hash n' cache
        vkDestroyPipelineLayout(device.handle(), _layout, nullptr);
    }

    auto pipeline_t::make(
        const framebuffer_t& framebuffer,
        const graphics_pipeline_create_info_t& info
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto pipeline = ir::arc_ptr<self>(new self(info));
        const auto& device = framebuffer.render_pass().device();
        IR_ASSERT(!info.vertex.empty(), "cannot create graphics pipeline without vertex shader");

        auto shader_stages = std::vector<VkPipelineShaderStageCreateInfo>();

        { // compile vertex stage
            const auto binary = compile_shader(info.vertex, shaderc_glsl_vertex_shader, device.logger());
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
            // TODO: reflect all resources
            const auto resources = compiler.get_shader_resources();

            auto vertex_stage_info = VkPipelineShaderStageCreateInfo();
            vertex_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertex_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertex_stage_info.pName = "main";

            auto module_info = VkShaderModuleCreateInfo();
            module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_info.codeSize = size_bytes(binary);
            module_info.pCode = binary.data();
            IR_VULKAN_CHECK(device.logger(), vkCreateShaderModule(device.handle(), &module_info, nullptr, &vertex_stage_info.module));
            shader_stages.emplace_back(vertex_stage_info);
        }

        auto color_blend_attachments = std::vector<VkPipelineColorBlendAttachmentState>();
        if (!info.fragment.empty()) { // compile fragment stage
            const auto binary = compile_shader(info.fragment, shaderc_glsl_fragment_shader, device.logger());
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
            // TODO: reflect all resources
            const auto resources = compiler.get_shader_resources();

            auto fragment_shader_info = VkPipelineShaderStageCreateInfo();
            fragment_shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragment_shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragment_shader_info.pName = "main";

            auto module_info = VkShaderModuleCreateInfo();
            module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_info.codeSize = size_bytes(binary);
            module_info.pCode = binary.data();
            IR_VULKAN_CHECK(device.logger(), vkCreateShaderModule(device.handle(), &module_info, nullptr, &fragment_shader_info.module));
            shader_stages.emplace_back(fragment_shader_info);

            for (auto i = 0_u32; i < resources.stage_outputs.size(); ++i) {
                const auto& type = compiler.get_type(resources.stage_outputs[i].type_id);
                auto color_blend_attachment = VkPipelineColorBlendAttachmentState();
                if (info.blend.empty()) {
                    color_blend_attachment.blendEnable = type.vecsize == 4;
                } else {
                    switch (info.blend[i]) {
                        case attachment_blend_t::e_auto:
                            color_blend_attachment.blendEnable = type.vecsize == 4;
                            break;
                        case attachment_blend_t::e_disabled:
                            color_blend_attachment.blendEnable = false;
                            break;
                    }
                }
                color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
                color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
                switch (type.vecsize) {
                    case 4: color_blend_attachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT; IR_FALLTHROUGH;
                    case 3: color_blend_attachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT; IR_FALLTHROUGH;
                    case 2: color_blend_attachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT; IR_FALLTHROUGH;
                    case 1: color_blend_attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
                }
                color_blend_attachments.emplace_back(color_blend_attachment);
            }
        }

        auto vertex_input_info = VkPipelineVertexInputStateCreateInfo();
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.pNext = nullptr;
        vertex_input_info.flags = 0;
        // TODO
        vertex_input_info.vertexBindingDescriptionCount = 0;
        vertex_input_info.pVertexBindingDescriptions = nullptr;
        vertex_input_info.vertexAttributeDescriptionCount = 0;
        vertex_input_info.pVertexAttributeDescriptions = nullptr;

        auto input_assembly_info = VkPipelineInputAssemblyStateCreateInfo();
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.pNext = nullptr;
        input_assembly_info.flags = 0;
        input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_info.primitiveRestartEnable = false;

        auto viewport = VkViewport();
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(framebuffer.width());
        viewport.height = static_cast<float>(framebuffer.height());
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        auto scissor = VkRect2D();
        scissor.offset = { 0, 0 };
        scissor.extent = {
            static_cast<uint32>(viewport.width),
            static_cast<uint32>(viewport.height)
        };

        auto viewport_info = VkPipelineViewportStateCreateInfo();
        viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_info.pNext = nullptr;
        viewport_info.flags = 0;
        viewport_info.viewportCount = 1;
        viewport_info.pViewports = &viewport;
        viewport_info.scissorCount = 1;
        viewport_info.pScissors = &scissor;

        auto rasterization_info = VkPipelineRasterizationStateCreateInfo();
        rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_info.pNext = nullptr;
        rasterization_info.flags = 0;
        rasterization_info.depthClampEnable = static_cast<bool>(
            as_underlying(info.depth_flags & depth_state_flag_t::e_enable_clamp));
        rasterization_info.rasterizerDiscardEnable = false;
        rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_info.cullMode = as_enum_counterpart(info.cull_mode);
        rasterization_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization_info.depthBiasEnable = false;
        rasterization_info.depthBiasConstantFactor = 0.0f;
        rasterization_info.depthBiasClamp = 0.0f;
        rasterization_info.depthBiasSlopeFactor = 0.0f;
        rasterization_info.lineWidth = 1.0f;

        auto multisample_info = VkPipelineMultisampleStateCreateInfo();
        multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_info.pNext = nullptr;
        multisample_info.flags = 0;
        multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_info.sampleShadingEnable = true;
        multisample_info.minSampleShading = 0.2f;
        multisample_info.pSampleMask = nullptr;
        multisample_info.alphaToCoverageEnable = false;
        multisample_info.alphaToOneEnable = false;

        auto depth_stencil_info = VkPipelineDepthStencilStateCreateInfo();
        depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_info.pNext = nullptr;
        depth_stencil_info.flags = 0;
        depth_stencil_info.depthTestEnable = static_cast<bool>(
            as_underlying(info.depth_flags & depth_state_flag_t::e_enable_test));
        depth_stencil_info.depthWriteEnable = static_cast<bool>(
            as_underlying(info.depth_flags & depth_state_flag_t::e_enable_write));
        depth_stencil_info.depthCompareOp = as_enum_counterpart(info.depth_compare_op);
        depth_stencil_info.depthBoundsTestEnable = false;
        depth_stencil_info.stencilTestEnable = false;
        depth_stencil_info.front = {};
        depth_stencil_info.back = {};
        depth_stencil_info.minDepthBounds = 0.0f;
        depth_stencil_info.maxDepthBounds = 1.0f;

        auto color_blend_info = VkPipelineColorBlendStateCreateInfo();
        color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_info.pNext = nullptr;
        color_blend_info.flags = 0;
        color_blend_info.logicOpEnable = false;
        color_blend_info.logicOp = VK_LOGIC_OP_COPY;
        color_blend_info.attachmentCount = color_blend_attachments.size();
        color_blend_info.pAttachments = color_blend_attachments.data();
        color_blend_info.blendConstants[0] = 0.0f;
        color_blend_info.blendConstants[1] = 0.0f;
        color_blend_info.blendConstants[2] = 0.0f;
        color_blend_info.blendConstants[3] = 0.0f;

        auto dynamic_states = std::vector<VkDynamicState>();
        dynamic_states.reserve(info.dynamic_states.size());
        for (auto state : info.dynamic_states) {
            dynamic_states.emplace_back(as_enum_counterpart(state));
        }

        auto dynamic_state_info = VkPipelineDynamicStateCreateInfo();
        dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_info.pNext = nullptr;
        dynamic_state_info.flags = 0;
        dynamic_state_info.dynamicStateCount = dynamic_states.size();
        dynamic_state_info.pDynamicStates = dynamic_states.data();

        // TODO
        auto pipeline_layout_info = VkPipelineLayoutCreateInfo();
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.flags = 0;
        pipeline_layout_info.setLayoutCount = 0;
        pipeline_layout_info.pSetLayouts = nullptr;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;
        IR_VULKAN_CHECK(device.logger(), vkCreatePipelineLayout(device.handle(), &pipeline_layout_info, nullptr, &pipeline->_layout));

        auto pipeline_info = VkGraphicsPipelineCreateInfo();
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = nullptr;
        pipeline_info.flags = 0;
        pipeline_info.stageCount = shader_stages.size();
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly_info;
        pipeline_info.pTessellationState = nullptr;
        pipeline_info.pViewportState = &viewport_info;
        pipeline_info.pRasterizationState = &rasterization_info;
        pipeline_info.pMultisampleState = &multisample_info;
        pipeline_info.pDepthStencilState = &depth_stencil_info;
        pipeline_info.pColorBlendState = &color_blend_info;
        pipeline_info.pDynamicState = &dynamic_state_info;
        pipeline_info.layout = pipeline->_layout;
        pipeline_info.renderPass = framebuffer.render_pass().handle();
        pipeline_info.subpass = info.subpass;
        pipeline_info.basePipelineHandle = nullptr;
        pipeline_info.basePipelineIndex = 0;
        IR_VULKAN_CHECK(device.logger(), vkCreateGraphicsPipelines(device.handle(), nullptr, 1, &pipeline_info, nullptr, &pipeline->_handle));
        if (info.fragment.empty()) {
            IR_LOG_INFO(device.logger(), "compiled pipeline: ({}, null)", info.vertex.generic_string());
        } else {
            IR_LOG_INFO(device.logger(), "compiled pipeline: ({}, {})", info.vertex.generic_string(), info.fragment.generic_string());
        }

        for (const auto& stage : shader_stages) {
            vkDestroyShaderModule(device.handle(), stage.module, nullptr);
        }

        pipeline->_type = pipeline_type_t::e_graphics;
        pipeline->_framebuffer = framebuffer.as_intrusive_ptr();
        return pipeline;
    }

    auto pipeline_t::handle() const noexcept -> VkPipeline {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto pipeline_t::layout() const noexcept -> VkPipelineLayout {
        IR_PROFILE_SCOPED();
        return _layout;
    }

    auto pipeline_t::type() const noexcept -> pipeline_type_t {
        IR_PROFILE_SCOPED();
        return _type;
    }

    auto pipeline_t::info() const noexcept -> const graphics_pipeline_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto pipeline_t::framebuffer() const noexcept -> const framebuffer_t& {
        IR_PROFILE_SCOPED();
        return *_framebuffer;
    }
}
