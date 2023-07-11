#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <iris/gfx/image.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <optional>
#include <vector>
#include <memory>
#include <span>

namespace ir {
    struct command_buffer_create_info_t {
        bool primary = true;
    };

    struct draw_mesh_tasks_indirect_command_t {
        uint32 x = 0;
        uint32 y = 0;
        uint32 z = 0;
    };

    struct memory_barrier_t {
        pipeline_stage_t source_stage = pipeline_stage_t::e_none;
        pipeline_stage_t dest_stage = pipeline_stage_t::e_none;
        resource_access_t source_access = resource_access_t::e_none;
        resource_access_t dest_access = resource_access_t::e_none;
    };

    struct buffer_memory_barrier_t {
        buffer_info_t buffer = {};
        pipeline_stage_t source_stage = pipeline_stage_t::e_none;
        pipeline_stage_t dest_stage = pipeline_stage_t::e_none;
        resource_access_t source_access = resource_access_t::e_none;
        resource_access_t dest_access = resource_access_t::e_none;
    };

    struct image_memory_barrier_t {
        std::reference_wrapper<const image_t> image;
        pipeline_stage_t source_stage = pipeline_stage_t::e_none;
        pipeline_stage_t dest_stage = pipeline_stage_t::e_none;
        resource_access_t source_access = resource_access_t::e_none;
        resource_access_t dest_access = resource_access_t::e_none;
        image_layout_t old_layout = image_layout_t::e_undefined;
        image_layout_t new_layout = image_layout_t::e_undefined;
        image_subresource_t subresource = {};
    };

    struct image_copy_t {
        offset_3d_t source_offset = ignored_offset_3d;
        offset_3d_t dest_offset = ignored_offset_3d;
        image_subresource_t source_subresource = {};
        image_subresource_t dest_subresource = {};
        extent_3d_t extent = ignored_extent_3d;
    };

    struct buffer_copy_t {
        uint32 source_offset = 0;
        uint32 dest_offset = 0;
    };

    struct viewport_t {
        float32 x = 0.0f;
        float32 y = 0.0f;
        float32 width = 0.0f;
        float32 height = 0.0f;
    };

    struct scissor_t {
        int32 x = 0;
        int32 y = 0;
        uint32 width = 0;
        uint32 height = 0;
    };

    class command_buffer_t : public enable_intrusive_refcount_t<command_buffer_t> {
    public:
        using self = command_buffer_t;

        command_buffer_t() noexcept;
        ~command_buffer_t() noexcept;

        IR_NODISCARD static auto make(
            const command_pool_t& pool,
            const command_buffer_create_info_t& info
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD static auto make(
            const command_pool_t& pool,
            uint32 count,
            const command_buffer_create_info_t& info
        ) noexcept -> std::vector<arc_ptr<self>>;

        IR_NODISCARD auto handle() const noexcept -> VkCommandBuffer;
        IR_NODISCARD auto info() const noexcept -> const command_buffer_create_info_t&;
        IR_NODISCARD auto pool() const noexcept -> const command_pool_t&;

        auto begin() noexcept -> void;
        auto begin_debug_marker(const std::string& name) noexcept -> void;
        auto end_debug_marker() noexcept -> void;
        auto begin_render_pass(const framebuffer_t& framebuffer, const std::vector<clear_value_t>& clears) noexcept -> void;
        auto set_viewport(const viewport_t& viewport) const noexcept -> void;
        auto set_scissor(const scissor_t& scissor) const noexcept -> void;
        auto bind_pipeline(const pipeline_t& pipeline) noexcept -> void;
        auto bind_descriptor_set(const descriptor_set_t& set) noexcept -> void;
        auto bind_vertex_buffer(const buffer_info_t& buffer) const noexcept -> void;
        auto bind_index_buffer(const buffer_info_t& buffer, index_type_t type = index_type_t::e_uint32) const noexcept -> void;
        auto push_constants(shader_stage_t stage, uint32 offset, uint64 size, const void* data) const noexcept -> void;
        auto draw(uint32 vertices, uint32 instances, uint32 first_vertex, uint32 first_instance) const noexcept -> void;
        auto draw_indexed(uint32 indices, uint32 instances, uint32 first_index, int32 vertex_offset, uint32 first_instance) const noexcept -> void;
        auto draw_mesh_tasks(uint32 x = 1, uint32 y = 1, uint32 z = 1) const noexcept -> void;
        auto draw_mesh_tasks_indirect(const buffer_info_t& buffer, uint32 count) const noexcept -> void;
        auto end_render_pass() noexcept -> void;
        auto dispatch(uint32 x = 1, uint32 y = 1, uint32 z = 1) const noexcept -> void;
        auto dispatch_indirect(const buffer_info_t& buffer) const noexcept -> void;
        auto fill_buffer(const buffer_info_t& buffer, uint32 data) const noexcept -> void;
        auto clear_image(const image_t& image, const clear_value_t& clear, const image_subresource_t& subresource) const noexcept -> void;
        auto copy_image(const image_t& source, const image_t& dest, const image_copy_t& copy) const noexcept -> void;
        auto copy_buffer(const buffer_info_t& source, const buffer_info_t& dest, const buffer_copy_t& copy) const noexcept -> void;
        auto copy_buffer_to_image(const buffer_info_t& source, const image_t& dest, const image_subresource_t& subresource) const noexcept -> void;
        auto memory_barrier(const memory_barrier_t& barrier) const noexcept -> void;
        auto buffer_barrier(const buffer_memory_barrier_t& barrier) const noexcept -> void;
        auto image_barrier(const image_memory_barrier_t& barrier) const noexcept -> void;

        auto end() const noexcept -> void;

    private:
        VkCommandBuffer _handle = {};

        struct {
            const framebuffer_t* framebuffer = nullptr;
            const pipeline_t* pipeline = nullptr;
        } _state;

        command_buffer_create_info_t _info = {};
        arc_ptr<const command_pool_t> _pool;
    };
}
