#include <iris/gfx/device.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/descriptor_layout.hpp>
#include <iris/gfx/render_pass.hpp>
#include <iris/gfx/framebuffer.hpp>
#include <iris/gfx/pipeline.hpp>

#include <iris/core/utilities.hpp>

#include <shaderc/shaderc.hpp>

#include <spirv_glsl.hpp>
#include <spirv.hpp>

#include <mio/mmap.hpp>

#include <volk.h>

#include <algorithm>
#include <utility>
#include <regex>
#include <numeric>
#include <fstream>

namespace ir {
    namespace spvc = spirv_cross;
    namespace shc = shaderc;

    using descriptor_bindings = akl::fast_hash_map<uint32, akl::fast_hash_map<uint32, descriptor_binding_t>>;

    static auto split_decoration_string(const std::string& decoration) -> std::vector<std::string> {
        const auto regex = std::regex(R"(\|)");
        const auto iterator = std::sregex_token_iterator(decoration.begin(), decoration.end(), regex, -1);
        return { iterator, {} };
    }

    static auto make_descriptor_binding_flag_from_decoration(const std::string& decoration) -> descriptor_binding_flag_t {
        const auto split = split_decoration_string(decoration);
        auto result = descriptor_binding_flag_t();
        for (const auto& each : split) {
            if (each == "update_after_bind") {
                result |= ir::descriptor_binding_flag_t::e_update_after_bind;
            } else if (each == "update_unused_while_pending") {
                result |= ir::descriptor_binding_flag_t::e_update_unused_while_pending;
            } else if (each == "partially_bound") {
                result |= ir::descriptor_binding_flag_t::e_partially_bound;
            } else if (each == "variable_descriptor_count") {
                result |= ir::descriptor_binding_flag_t::e_variable_descriptor_count;
            }
        }
        return result;
    }

    template <typename R>
    static auto process_resource(
        const spvc::Compiler& compiler,
        const R& resources,
        descriptor_type_t desc_type,
        shader_stage_t stage,
        descriptor_bindings& bindings
    ) noexcept -> void {
        for (const auto& each : resources) {
            const auto& set = compiler.get_decoration(each.id, spv::DecorationDescriptorSet);
            const auto& binding = compiler.get_decoration(each.id, spv::DecorationBinding);
            const auto& decoration = compiler.get_decoration_string(each.id, spv::DecorationUserSemantic);
            const auto& type = compiler.get_type(each.type_id);
            auto binding_flags = make_descriptor_binding_flag_from_decoration(decoration);
            // TODO: multidimensional arrays?
            auto count = type.array.empty() ? 1 : type.array[0];
            const auto is_array = !type.array.empty();
            const auto is_dynamic = is_array && type.array[0] == 0;
            if (is_dynamic) {
                count = 16384_u32;
                binding_flags |=
                    descriptor_binding_flag_t::e_update_after_bind |
                    descriptor_binding_flag_t::e_partially_bound |
                    descriptor_binding_flag_t::e_variable_descriptor_count;
            }
            auto& layout = bindings[set];
            if (layout.contains(binding)) {
                layout[binding].stage |= stage;
            } else {
                layout[binding] = {
                    .set = set,
                    .binding = binding,
                    .count = count,
                    .type = desc_type,
                    .stage = stage,
                    .flags = binding_flags,
                    .is_dynamic = is_dynamic,
                };
            }
        }
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
            auto ec = std::error_code();
            auto file = mio::make_mmap_source(path.generic_string(), ec);
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
        auto ec = std::error_code();
        const auto file = mio::make_mmap_source(path.generic_string(), ec);
        auto compiler = shc::Compiler();
        auto include_path = path.parent_path();
        while (include_path.filename().generic_string() != "shaders") {
            include_path = include_path.parent_path();
        }
        auto options = shc::CompileOptions();
        options.SetGenerateDebugInfo();
        options.SetOptimizationLevel(shaderc_optimization_level_zero);
        options.SetSourceLanguage(shaderc_source_language_glsl);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        options.SetTargetSpirv(shaderc_spirv_version_1_6);
        options.SetIncluder(std::make_unique<shader_includer_t>(std::move(include_path)));
        auto spirv = compiler.CompileGlslToSpv(file.data(), file.size(), kind, path.string().c_str(), options);
        if (spirv.GetCompilationStatus() != shaderc_compilation_status_success) {
            logger.error("shader compile failed:\n\"{}\"", spirv.GetErrorMessage());
            logger.flush();
            return {};
        }
        return { spirv.cbegin(), spirv.cend() };
    }

    pipeline_t::pipeline_t() noexcept = default;

    pipeline_t::~pipeline_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroyPipeline(device().handle(), _handle, nullptr);
        vkDestroyPipelineLayout(device().handle(), _layout, nullptr);
    }

    auto pipeline_t::make(device_t& device, const compute_pipeline_create_info_t& info) noexcept -> arc_ptr<self> {
        auto pipeline = arc_ptr<self>(new self());
        IR_ASSERT(!info.compute.empty(), "compute shader must be specified");

        auto shader_stages = std::vector<VkPipelineShaderStageCreateInfo>();
        auto desc_bindings = descriptor_bindings();
        auto push_constant_info = std::vector<VkPushConstantRange>();
        const auto binary = compile_shader(info.compute, shaderc_glsl_compute_shader, device.logger());
        const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
        const auto resources = compiler.get_shader_resources();

        auto compute_stage_info = VkPipelineShaderStageCreateInfo();
        compute_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compute_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compute_stage_info.pName = "main";

        auto module_info = VkShaderModuleCreateInfo();
        module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        module_info.codeSize = size_bytes(binary);
        module_info.pCode = binary.data();
        IR_VULKAN_CHECK(device.logger(), vkCreateShaderModule(device.handle(), &module_info, nullptr, &compute_stage_info.module));
        shader_stages.emplace_back(compute_stage_info);

        process_resource(
            compiler,
            resources.uniform_buffers,
            descriptor_type_t::e_uniform_buffer,
            shader_stage_t::e_compute,
            desc_bindings);
        process_resource(
            compiler,
            resources.storage_buffers,
            descriptor_type_t::e_storage_buffer,
            shader_stage_t::e_compute,
            desc_bindings);
        process_resource(
            compiler,
            resources.storage_images,
            descriptor_type_t::e_storage_image,
            shader_stage_t::e_compute,
            desc_bindings);
        process_resource(
            compiler,
            resources.sampled_images,
            descriptor_type_t::e_combined_image_sampler,
            shader_stage_t::e_compute,
            desc_bindings);
        process_resource(
            compiler,
            resources.separate_images,
            descriptor_type_t::e_sampled_image,
            shader_stage_t::e_compute,
            desc_bindings);
        process_resource(
            compiler,
            resources.separate_samplers,
            descriptor_type_t::e_sampler,
            shader_stage_t::e_compute,
            desc_bindings);

        if (!resources.push_constant_buffers.empty()) {
            const auto& pc = resources.push_constant_buffers.front();
            const auto& type = compiler.get_type(pc.type_id);
            push_constant_info.emplace_back(VkPushConstantRange {
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = static_cast<uint32>(compiler.get_declared_struct_size(type))
            });
        }

        const auto max_set = std::max_element(
            desc_bindings.begin(),
            desc_bindings.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.first < rhs.first;
            })->first;
        auto descriptor_layout = std::vector<arc_ptr<descriptor_layout_t>>();
        descriptor_layout.resize(1);
        if (desc_bindings.empty()) {
            desc_bindings[0] = {}; // dummy layout
        }
        {
            auto& cache = device.cache<descriptor_layout_t>();
            for (const auto& [set, layout] : desc_bindings) {
                const auto& pair_bindings = layout.values();
                auto bindings = std::vector<descriptor_binding_t>(
                    std::max_element(
                        pair_bindings.begin(),
                        pair_bindings.end(),
                        [](const auto& lhs, const auto& rhs) {
                            return lhs.first < rhs.first;
                        })->first + 1);
                for (const auto& [binding, desc] : pair_bindings) {
                    bindings[binding] = desc;
                }
                if (cache.contains(bindings)) {
                    descriptor_layout[set] = cache.acquire(bindings);
                } else {
                    descriptor_layout[set] = cache.insert(bindings, descriptor_layout_t::make(device, {
                        .bindings = bindings
                    }));
                }
            }
        }
        descriptor_layout.erase(
            std::remove_if(
                descriptor_layout.begin(),
                descriptor_layout.end(),
                [](const auto& layout) {
                    return !layout;
                }),
            descriptor_layout.end());

        auto descriptor_layout_handles = std::vector<VkDescriptorSetLayout>();
        descriptor_layout_handles.reserve(descriptor_layout.size());
        std::transform(
            descriptor_layout.begin(),
            descriptor_layout.end(),
            std::back_inserter(descriptor_layout_handles),
            [](const auto& layout) {
                return layout->handle();
            });

        auto pipeline_layout_info = VkPipelineLayoutCreateInfo();
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.flags = 0;
        pipeline_layout_info.setLayoutCount = descriptor_layout_handles.size();
        pipeline_layout_info.pSetLayouts = descriptor_layout_handles.data();
        pipeline_layout_info.pushConstantRangeCount = push_constant_info.size();
        pipeline_layout_info.pPushConstantRanges = push_constant_info.data();
        IR_VULKAN_CHECK(device.logger(), vkCreatePipelineLayout(device.handle(), &pipeline_layout_info, nullptr, &pipeline->_layout));

        auto pipeline_info = VkComputePipelineCreateInfo();
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = nullptr;
        pipeline_info.flags = 0;
        pipeline_info.stage = compute_stage_info;
        pipeline_info.layout = pipeline->_layout;
        pipeline_info.basePipelineHandle = {};
        pipeline_info.basePipelineIndex = 0;
        IR_VULKAN_CHECK(device.logger(), vkCreateComputePipelines(device.handle(), {}, 1, &pipeline_info, nullptr, &pipeline->_handle));
        IR_LOG_INFO(device.logger(), "compiled compute pipeline: ({})", info.compute.generic_string());

        for (const auto& stage : shader_stages) {
            vkDestroyShaderModule(device.handle(), stage.module, nullptr);
        }

        pipeline->_descriptor_layout = std::move(descriptor_layout);
        pipeline->_type = pipeline_type_t::e_compute;
        pipeline->_info = info;
        pipeline->_device = device.as_intrusive_ptr();

        if (!info.name.empty()) {
            device.set_debug_name({
                .type = VK_OBJECT_TYPE_PIPELINE,
                .handle = reinterpret_cast<uint64>(pipeline->_handle),
                .name = info.name.c_str(),
            });
        }
        return pipeline;
    }

    auto pipeline_t::make(
        device_t& device,
        const render_pass_t& render_pass,
        const graphics_pipeline_create_info_t& info
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto pipeline = arc_ptr<self>(new self());
        IR_ASSERT(!info.vertex.empty(), "cannot create graphics pipeline without vertex shader");

        auto shader_stages = std::vector<VkPipelineShaderStageCreateInfo>();
        auto desc_bindings = descriptor_bindings();

        auto push_constant_info = std::vector<VkPushConstantRange>();

        { // compile vertex stage
            const auto binary = compile_shader(info.vertex, shaderc_glsl_vertex_shader, device.logger());
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
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

            process_resource(
                compiler,
                resources.uniform_buffers,
                descriptor_type_t::e_uniform_buffer,
                shader_stage_t::e_vertex,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_buffers,
                descriptor_type_t::e_storage_buffer,
                shader_stage_t::e_vertex,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_images,
                descriptor_type_t::e_storage_image,
                shader_stage_t::e_vertex,
                desc_bindings);
            process_resource(
                compiler,
                resources.sampled_images,
                descriptor_type_t::e_combined_image_sampler,
                shader_stage_t::e_vertex,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_images,
                descriptor_type_t::e_sampled_image,
                shader_stage_t::e_vertex,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_samplers,
                descriptor_type_t::e_sampler,
                shader_stage_t::e_vertex,
                desc_bindings);

            if (!resources.push_constant_buffers.empty()) {
                const auto& pc = resources.push_constant_buffers.front();
                const auto& type = compiler.get_type(pc.type_id);
                push_constant_info.emplace_back(VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                    .offset = 0,
                    .size = static_cast<uint32>(compiler.get_declared_struct_size(type))
                });
            }
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

            process_resource(
                compiler,
                resources.uniform_buffers,
                descriptor_type_t::e_uniform_buffer,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_buffers,
                descriptor_type_t::e_storage_buffer,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_images,
                descriptor_type_t::e_storage_image,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.sampled_images,
                descriptor_type_t::e_combined_image_sampler,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_images,
                descriptor_type_t::e_sampled_image,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_samplers,
                descriptor_type_t::e_sampler,
                shader_stage_t::e_fragment,
                desc_bindings);

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

            if (!resources.push_constant_buffers.empty()) {
                const auto& pc = resources.push_constant_buffers.front();
                const auto& type = compiler.get_type(pc.type_id);
                const auto size = compiler.get_declared_struct_size(type);
                auto* last = push_constant_info.empty() ? nullptr : &push_constant_info.back();
                if (push_constant_info.empty() || (last && last->size != size)) {
                    push_constant_info.emplace_back(VkPushConstantRange {
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = static_cast<uint32>(compiler.get_declared_struct_size(type))
                    });
                } else {
                    last->stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
                }
            }
        }

        auto vertex_binding_info = VkVertexInputBindingDescription();
        vertex_binding_info.binding = 0;
        vertex_binding_info.stride = 0;
        vertex_binding_info.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        auto vertex_attribute_info = std::vector<VkVertexInputAttributeDescription>();
        for (auto location = 0_u32; const auto& attribute : info.vertex_attributes) {
            const auto format = [&]() {
                switch (attribute) {
                    case vertex_attribute_t::e_vec1: return VK_FORMAT_R32_SFLOAT;
                    case vertex_attribute_t::e_vec2: return VK_FORMAT_R32G32_SFLOAT;
                    case vertex_attribute_t::e_vec3: return VK_FORMAT_R32G32B32_SFLOAT;
                    case vertex_attribute_t::e_vec4: return VK_FORMAT_R32G32B32A32_SFLOAT;
                }
                IR_UNREACHABLE();
            }();
            const auto size = as_underlying(attribute);
            auto attribute_info = VkVertexInputAttributeDescription();
            attribute_info.binding = 0;
            attribute_info.location = location++;
            attribute_info.format = format;
            attribute_info.offset = vertex_binding_info.stride;
            vertex_attribute_info.emplace_back(attribute_info);
            vertex_binding_info.stride += size;
        }

        auto vertex_input_info = VkPipelineVertexInputStateCreateInfo();
        vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_info.pNext = nullptr;
        vertex_input_info.flags = 0;
        if (!vertex_attribute_info.empty()) {
            vertex_input_info.vertexBindingDescriptionCount = 1;
            vertex_input_info.pVertexBindingDescriptions = &vertex_binding_info;
            vertex_input_info.vertexAttributeDescriptionCount = vertex_attribute_info.size();
            vertex_input_info.pVertexAttributeDescriptions = vertex_attribute_info.data();
        }

        auto input_assembly_info = VkPipelineInputAssemblyStateCreateInfo();
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.pNext = nullptr;
        input_assembly_info.flags = 0;
        input_assembly_info.topology = as_enum_counterpart(info.primitive_type);
        input_assembly_info.primitiveRestartEnable = false;

        auto viewport = VkViewport();
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float32>(info.width);
        viewport.height = static_cast<float32>(info.height);
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
        multisample_info.rasterizationSamples = as_enum_counterpart(info.sample_count);
        multisample_info.sampleShadingEnable = true;
        multisample_info.minSampleShading = 1.0f;
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

        auto descriptor_layout = std::vector<arc_ptr<descriptor_layout_t>>();
        descriptor_layout.reserve(desc_bindings.size());
        if (desc_bindings.empty()) {
            desc_bindings[0] = {}; // dummy layout
        }
        {
            auto& cache = device.cache<descriptor_layout_t>();
            for (const auto& [set, layout] : desc_bindings) {
                const auto& pair_bindings = layout.values();
                auto bindings = std::vector<descriptor_binding_t>();
                bindings.reserve(pair_bindings.size());
                std::transform(
                    pair_bindings.begin(),
                    pair_bindings.end(),
                    std::back_inserter(bindings),
                    [](const auto& pair) {
                        return pair.second;
                    });
                if (set >= descriptor_layout.size()) {
                    descriptor_layout.resize(set + 1);
                }
                if (cache.contains(bindings)) {
                    descriptor_layout[set] = cache.acquire(bindings);
                } else {
                    descriptor_layout[set] = cache.insert(bindings, descriptor_layout_t::make(device, {
                        .bindings = bindings
                    }));
                }
            }
        }
        descriptor_layout.erase(
            std::remove_if(
                descriptor_layout.begin(),
                descriptor_layout.end(),
                [](const auto& layout) {
                    return !layout;
                }),
            descriptor_layout.end());

        auto descriptor_layout_handles = std::vector<VkDescriptorSetLayout>();
        descriptor_layout_handles.reserve(descriptor_layout.size());
        std::transform(
            descriptor_layout.begin(),
            descriptor_layout.end(),
            std::back_inserter(descriptor_layout_handles),
            [](const auto& layout) {
                return layout->handle();
            });

        auto pipeline_layout_info = VkPipelineLayoutCreateInfo();
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.flags = 0;
        pipeline_layout_info.setLayoutCount = descriptor_layout_handles.size();
        pipeline_layout_info.pSetLayouts = descriptor_layout_handles.data();
        pipeline_layout_info.pushConstantRangeCount = push_constant_info.size();
        pipeline_layout_info.pPushConstantRanges = push_constant_info.data();
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
        pipeline_info.renderPass = render_pass.handle();
        pipeline_info.subpass = info.subpass;
        pipeline_info.basePipelineHandle = nullptr;
        pipeline_info.basePipelineIndex = 0;
        IR_VULKAN_CHECK(device.logger(), vkCreateGraphicsPipelines(device.handle(), nullptr, 1, &pipeline_info, nullptr, &pipeline->_handle));
        if (info.fragment.empty()) {
            IR_LOG_INFO(device.logger(), "compiled graphics pipeline: ({}, null)", info.vertex.generic_string());
        } else {
            IR_LOG_INFO(device.logger(), "compiled graphics pipeline: ({}, {})", info.vertex.generic_string(), info.fragment.generic_string());
        }

        for (const auto& stage : shader_stages) {
            vkDestroyShaderModule(device.handle(), stage.module, nullptr);
        }

        pipeline->_descriptor_layout = std::move(descriptor_layout);
        pipeline->_type = pipeline_type_t::e_graphics;
        pipeline->_info = info;
        pipeline->_device = device.as_intrusive_ptr();
        pipeline->_render_pass = render_pass.as_intrusive_ptr();

        if (!info.name.empty()) {
            device.set_debug_name({
                .type = VK_OBJECT_TYPE_PIPELINE,
                .handle = reinterpret_cast<uint64>(pipeline->_handle),
                .name = info.name.c_str(),
            });
        }
        return pipeline;
    }

    auto pipeline_t::make(
        device_t& device,
        const render_pass_t& render_pass,
        const mesh_shading_pipeline_create_info_t& info
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto pipeline = arc_ptr<self>(new self());
        IR_ASSERT(!info.mesh.empty(), "cannot create graphics pipeline without mesh shader");

        auto shader_stages = std::vector<VkPipelineShaderStageCreateInfo>();
        auto desc_bindings = descriptor_bindings();

        // TODO: hashmap
        auto push_constant_info = std::vector<VkPushConstantRange>();

        if (!info.task.empty()) { // compile task stage
            const auto binary = compile_shader(info.task, shaderc_glsl_task_shader, device.logger());
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
            const auto resources = compiler.get_shader_resources();

            auto task_stage_info = VkPipelineShaderStageCreateInfo();
            task_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            task_stage_info.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
            task_stage_info.pName = "main";

            auto module_info = VkShaderModuleCreateInfo();
            module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_info.codeSize = size_bytes(binary);
            module_info.pCode = binary.data();
            IR_VULKAN_CHECK(device.logger(), vkCreateShaderModule(device.handle(), &module_info, nullptr, &task_stage_info.module));
            shader_stages.emplace_back(task_stage_info);

            process_resource(
                compiler,
                resources.uniform_buffers,
                descriptor_type_t::e_uniform_buffer,
                shader_stage_t::e_task,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_buffers,
                descriptor_type_t::e_storage_buffer,
                shader_stage_t::e_task,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_images,
                descriptor_type_t::e_storage_image,
                shader_stage_t::e_task,
                desc_bindings);
            process_resource(
                compiler,
                resources.sampled_images,
                descriptor_type_t::e_combined_image_sampler,
                shader_stage_t::e_task,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_images,
                descriptor_type_t::e_sampled_image,
                shader_stage_t::e_task,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_samplers,
                descriptor_type_t::e_sampler,
                shader_stage_t::e_task,
                desc_bindings);

            if (!resources.push_constant_buffers.empty()) {
                const auto& pc = resources.push_constant_buffers.front();
                const auto& type = compiler.get_type(pc.type_id);
                push_constant_info.emplace_back(VkPushConstantRange {
                    .stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT,
                    .offset = 0,
                    .size = static_cast<uint32>(compiler.get_declared_struct_size(type))
                });
            }
        }

        { // compile mesh stage
            const auto binary = compile_shader(info.mesh, shaderc_glsl_mesh_shader, device.logger());
            const auto compiler = spvc::CompilerGLSL(binary.data(), binary.size());
            const auto resources = compiler.get_shader_resources();

            auto mesh_stage_info = VkPipelineShaderStageCreateInfo();
            mesh_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            mesh_stage_info.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
            mesh_stage_info.pName = "main";

            auto module_info = VkShaderModuleCreateInfo();
            module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            module_info.codeSize = size_bytes(binary);
            module_info.pCode = binary.data();
            IR_VULKAN_CHECK(device.logger(), vkCreateShaderModule(device.handle(), &module_info, nullptr, &mesh_stage_info.module));
            shader_stages.emplace_back(mesh_stage_info);

            process_resource(
                compiler,
                resources.uniform_buffers,
                descriptor_type_t::e_uniform_buffer,
                shader_stage_t::e_mesh,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_buffers,
                descriptor_type_t::e_storage_buffer,
                shader_stage_t::e_mesh,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_images,
                descriptor_type_t::e_storage_image,
                shader_stage_t::e_mesh,
                desc_bindings);
            process_resource(
                compiler,
                resources.sampled_images,
                descriptor_type_t::e_combined_image_sampler,
                shader_stage_t::e_mesh,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_images,
                descriptor_type_t::e_sampled_image,
                shader_stage_t::e_mesh,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_samplers,
                descriptor_type_t::e_sampler,
                shader_stage_t::e_mesh,
                desc_bindings);

            if (!resources.push_constant_buffers.empty()) {
                const auto& pc = resources.push_constant_buffers.front();
                const auto& type = compiler.get_type(pc.type_id);
                const auto size = compiler.get_declared_struct_size(type);
                auto* last = push_constant_info.empty() ? nullptr : &push_constant_info.back();
                if (push_constant_info.empty() || (last && last->size != size)) {
                    push_constant_info.emplace_back(VkPushConstantRange {
                        .stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT,
                        .offset = 0,
                        .size = static_cast<uint32>(compiler.get_declared_struct_size(type))
                    });
                } else {
                    last->stageFlags |= VK_SHADER_STAGE_MESH_BIT_EXT;
                }
            }
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

            process_resource(
                compiler,
                resources.uniform_buffers,
                descriptor_type_t::e_uniform_buffer,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_buffers,
                descriptor_type_t::e_storage_buffer,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.storage_images,
                descriptor_type_t::e_storage_image,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.sampled_images,
                descriptor_type_t::e_combined_image_sampler,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_images,
                descriptor_type_t::e_sampled_image,
                shader_stage_t::e_fragment,
                desc_bindings);
            process_resource(
                compiler,
                resources.separate_samplers,
                descriptor_type_t::e_sampler,
                shader_stage_t::e_fragment,
                desc_bindings);

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

            if (!resources.push_constant_buffers.empty()) {
                const auto& pc = resources.push_constant_buffers.front();
                const auto& type = compiler.get_type(pc.type_id);
                const auto size = compiler.get_declared_struct_size(type);
                auto* last = push_constant_info.empty() ? nullptr : &push_constant_info.back();
                if (push_constant_info.empty() || (last && last->size != size)) {
                    push_constant_info.emplace_back(VkPushConstantRange {
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = static_cast<uint32>(compiler.get_declared_struct_size(type))
                    });
                } else {
                    last->stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
                }
            }
        }

        auto viewport = VkViewport();
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float32>(info.width);
        viewport.height = static_cast<float32>(info.height);
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
        multisample_info.rasterizationSamples = as_enum_counterpart(info.sample_count);
        multisample_info.sampleShadingEnable = true;
        multisample_info.minSampleShading = 1.0f;
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

        auto descriptor_layout = std::vector<arc_ptr<descriptor_layout_t>>();
        descriptor_layout.reserve(desc_bindings.size());
        if (desc_bindings.empty()) {
            desc_bindings[0] = {}; // dummy layout
        }
        {
            auto& cache = device.cache<descriptor_layout_t>();
            for (const auto& [set, layout] : desc_bindings) {
                const auto& pair_bindings = layout.values();
                auto bindings = std::vector<descriptor_binding_t>();
                bindings.reserve(pair_bindings.size());
                std::transform(
                    pair_bindings.begin(),
                    pair_bindings.end(),
                    std::back_inserter(bindings),
                    [](const auto& pair) {
                        return pair.second;
                    });
                if (set >= descriptor_layout.size()) {
                    descriptor_layout.resize(set + 1);
                }
                if (cache.contains(bindings)) {
                    descriptor_layout[set] = cache.acquire(bindings);
                } else {
                    descriptor_layout[set] = cache.insert(bindings, descriptor_layout_t::make(device, {
                        .bindings = bindings
                    }));
                }
            }
        }
        descriptor_layout.erase(
            std::remove_if(
                descriptor_layout.begin(),
                descriptor_layout.end(),
                [](const auto& layout) {
                    return !layout;
                }),
            descriptor_layout.end());

        auto descriptor_layout_handles = std::vector<VkDescriptorSetLayout>();
        descriptor_layout_handles.reserve(descriptor_layout.size());
        std::transform(
            descriptor_layout.begin(),
            descriptor_layout.end(),
            std::back_inserter(descriptor_layout_handles),
            [](const auto& layout) {
                return layout->handle();
            });

        auto pipeline_layout_info = VkPipelineLayoutCreateInfo();
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.flags = 0;
        pipeline_layout_info.setLayoutCount = descriptor_layout_handles.size();
        pipeline_layout_info.pSetLayouts = descriptor_layout_handles.data();
        pipeline_layout_info.pushConstantRangeCount = push_constant_info.size();
        pipeline_layout_info.pPushConstantRanges = push_constant_info.data();
        IR_VULKAN_CHECK(device.logger(), vkCreatePipelineLayout(device.handle(), &pipeline_layout_info, nullptr, &pipeline->_layout));

        auto pipeline_info = VkGraphicsPipelineCreateInfo();
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = nullptr;
        pipeline_info.flags = 0;
        pipeline_info.stageCount = shader_stages.size();
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = nullptr;
        pipeline_info.pInputAssemblyState = nullptr;
        pipeline_info.pTessellationState = nullptr;
        pipeline_info.pViewportState = &viewport_info;
        pipeline_info.pRasterizationState = &rasterization_info;
        pipeline_info.pMultisampleState = &multisample_info;
        pipeline_info.pDepthStencilState = &depth_stencil_info;
        pipeline_info.pColorBlendState = &color_blend_info;
        pipeline_info.pDynamicState = &dynamic_state_info;
        pipeline_info.layout = pipeline->_layout;
        pipeline_info.renderPass = render_pass.handle();
        pipeline_info.subpass = info.subpass;
        pipeline_info.basePipelineHandle = {};
        pipeline_info.basePipelineIndex = 0;
        IR_VULKAN_CHECK(device.logger(), vkCreateGraphicsPipelines(device.handle(), nullptr, 1, &pipeline_info, nullptr, &pipeline->_handle));
        IR_LOG_INFO(
            device.logger(), "compiled mesh shading pipeline: ({}, {}, {})",
            info.task.empty() ? "null" : info.task.generic_string().c_str(),
            info.mesh.generic_string().c_str(),
            info.fragment.empty() ? "null" : info.fragment.generic_string().c_str());

        for (const auto& stage : shader_stages) {
            vkDestroyShaderModule(device.handle(), stage.module, nullptr);
        }

        pipeline->_descriptor_layout = std::move(descriptor_layout);
        pipeline->_type = pipeline_type_t::e_graphics;
        pipeline->_info = info;
        pipeline->_device = device.as_intrusive_ptr();
        pipeline->_render_pass = render_pass.as_intrusive_ptr();

        if (!info.name.empty()) {
            device.set_debug_name({
                .type = VK_OBJECT_TYPE_PIPELINE,
                .handle = reinterpret_cast<uint64>(pipeline->_handle),
                .name = info.name.c_str(),
            });
        }
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

    auto pipeline_t::descriptor_layouts() const noexcept -> std::span<const arc_ptr<descriptor_layout_t>> {
        IR_PROFILE_SCOPED();
        return _descriptor_layout;
    }

    auto pipeline_t::descriptor_layout(uint32 index) const noexcept -> const descriptor_layout_t& {
        IR_PROFILE_SCOPED();
        return *_descriptor_layout[index];
    }

    auto pipeline_t::descriptor_binding(const descriptor_reference& reference) const noexcept -> const descriptor_binding_t& {
        IR_PROFILE_SCOPED();
        const auto [set, binding] = unpack_descriptor_reference(reference);
        return descriptor_layout(set).binding(binding);
    }

    auto pipeline_t::type() const noexcept -> pipeline_type_t {
        IR_PROFILE_SCOPED();
        return _type;
    }

    auto pipeline_t::compute_info() const noexcept -> const compute_pipeline_create_info_t& {
        return std::get<0>(_info);
    }

    auto pipeline_t::graphics_info() const noexcept -> const graphics_pipeline_create_info_t& {
        IR_PROFILE_SCOPED();
        return std::get<1>(_info);
    }

    auto pipeline_t::mesh_info() const noexcept -> const mesh_shading_pipeline_create_info_t& {
        IR_PROFILE_SCOPED();
        return std::get<2>(_info);
    }

    auto pipeline_t::device() noexcept -> device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }

    auto pipeline_t::render_pass() const noexcept -> const render_pass_t& {
        IR_PROFILE_SCOPED();
        return *_render_pass;
    }
}
