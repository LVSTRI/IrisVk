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
        e_none = 0x0,
        e_enable_clamp = 0x1,
        e_enable_test = 0x2,
        e_enable_write = 0x4,
    };

    struct graphics_pipeline_create_info_t {
        fs::path vertex;
        fs::path fragment;
        std::vector<attachment_blend_t> blend;
        std::vector<dynamic_state_t> dynamic_states;
        depth_state_flag_t depth_flags = {};
        compare_op_t depth_compare_op = compare_op_t::e_less;
        cull_mode_t cull_mode = cull_mode_t::e_none;
        uint32 subpass = 0;
    };

    class pipeline_t : public enable_intrusive_refcount_t<pipeline_t> {
    public:
        using self = pipeline_t;

        pipeline_t(const graphics_pipeline_create_info_t& info) noexcept;
        ~pipeline_t() noexcept;

        IR_NODISCARD static auto make(device_t& device, const framebuffer_t& framebuffer, const graphics_pipeline_create_info_t& info) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkPipeline;
        IR_NODISCARD auto layout() const noexcept -> VkPipelineLayout;
        IR_NODISCARD auto descriptor_layouts() const noexcept -> std::span<const arc_ptr<descriptor_layout_t>>;
        IR_NODISCARD auto descriptor_layout(uint32 index) const noexcept -> const descriptor_layout_t&;
        IR_NODISCARD auto descriptor_binding(const descriptor_reference& reference) const noexcept -> const descriptor_binding_t&;

        IR_NODISCARD auto type() const noexcept -> pipeline_type_t;
        IR_NODISCARD auto info() const noexcept -> const graphics_pipeline_create_info_t&;
        IR_NODISCARD auto framebuffer() const noexcept -> const framebuffer_t&;

    private:
        VkPipeline _handle;
        VkPipelineLayout _layout;
        std::vector<arc_ptr<descriptor_layout_t>> _descriptor_layout;
        pipeline_type_t _type = {};

        graphics_pipeline_create_info_t _info;
        arc_ptr<device_t> _device;
        arc_ptr<const framebuffer_t> _framebuffer;
    };
}