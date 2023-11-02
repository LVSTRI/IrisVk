#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <optional>
#include <vector>
#include <memory>
#include <string>
#include <span>

namespace ir {
    enum class pipeline_type_t {
        e_graphics,
        e_compute,
        e_ray_tracing
    };

    enum class attachment_blend_t {
        e_auto,
        e_disabled
    };

    enum class depth_state_flag_t {
        e_none = 0,
        e_enable_clamp = 1 << 0,
        e_enable_test = 1 << 1,
        e_enable_write = 1 << 2,
    };

    enum class vertex_attribute_t {
        e_vec1 = sizeof(float32[1]),
        e_vec2 = sizeof(float32[2]),
        e_vec3 = sizeof(float32[3]),
        e_vec4 = sizeof(float32[4]),
    };

    struct compute_pipeline_create_info_t {
        std::string name = {};
        fs::path compute;
        // TODO
    };

    struct graphics_pipeline_create_info_t {
        std::string name = {};
        fs::path vertex;
        fs::path fragment;
        sample_count_t sample_count = sample_count_t::e_1;
        primitive_topology_t primitive_type = primitive_topology_t::e_triangle_list;
        std::vector<attachment_blend_t> blend;
        std::vector<dynamic_state_t> dynamic_states;
        std::vector<vertex_attribute_t> vertex_attributes;
        depth_state_flag_t depth_flags = {};
        compare_op_t depth_compare_op = compare_op_t::e_less;
        cull_mode_t cull_mode = cull_mode_t::e_none;
        uint32 width = 0;
        uint32 height = 0;
        uint32 subpass = 0;
    };

    struct mesh_shading_pipeline_create_info_t {
        std::string name = {};
        fs::path task;
        fs::path mesh;
        fs::path fragment;
        sample_count_t sample_count = sample_count_t::e_1;
        std::vector<attachment_blend_t> blend;
        std::vector<dynamic_state_t> dynamic_states;
        depth_state_flag_t depth_flags = {};
        compare_op_t depth_compare_op = compare_op_t::e_less;
        cull_mode_t cull_mode = cull_mode_t::e_none;
        uint32 width = 0;
        uint32 height = 0;
        uint32 subpass = 0;
    };

    class pipeline_t : public enable_intrusive_refcount_t<pipeline_t> {
    public:
        using self = pipeline_t;

        pipeline_t() noexcept;
        ~pipeline_t() noexcept;

        IR_NODISCARD static auto make(device_t& device, const compute_pipeline_create_info_t& info) noexcept -> arc_ptr<self>;
        IR_NODISCARD static auto make(
            device_t& device,
            const render_pass_t& render_pass,
            const graphics_pipeline_create_info_t& info
        ) noexcept -> arc_ptr<self>;
        IR_NODISCARD static auto make(
            device_t& device,
            const render_pass_t& render_pass,
            const mesh_shading_pipeline_create_info_t& info
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkPipeline;
        IR_NODISCARD auto layout() const noexcept -> VkPipelineLayout;
        IR_NODISCARD auto descriptor_layouts() const noexcept -> std::span<const arc_ptr<descriptor_layout_t>>;
        IR_NODISCARD auto descriptor_layout(uint32 index) const noexcept -> const descriptor_layout_t&;
        IR_NODISCARD auto descriptor_binding(const descriptor_reference& reference) const noexcept -> const descriptor_binding_t&;

        IR_NODISCARD auto type() const noexcept -> pipeline_type_t;
        IR_NODISCARD auto compute_info() const noexcept -> const compute_pipeline_create_info_t&;
        IR_NODISCARD auto graphics_info() const noexcept -> const graphics_pipeline_create_info_t&;
        IR_NODISCARD auto mesh_info() const noexcept -> const mesh_shading_pipeline_create_info_t&;
        IR_NODISCARD auto device() noexcept -> device_t&;
        IR_NODISCARD auto render_pass() const noexcept -> const render_pass_t&;

    private:
        VkPipeline _handle = {};
        VkPipelineLayout _layout = {};
        std::vector<arc_ptr<descriptor_layout_t>> _descriptor_layout;
        pipeline_type_t _type = {};

        std::variant<
            compute_pipeline_create_info_t,
            graphics_pipeline_create_info_t,
            mesh_shading_pipeline_create_info_t> _info = {};
        arc_ptr<device_t> _device;
        arc_ptr<const render_pass_t> _render_pass;
    };
}