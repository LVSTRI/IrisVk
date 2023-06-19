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

    struct memory_barrier_t {
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
        std::reference_wrapper<const image_t> source;
        std::reference_wrapper<const image_t> dest;
        offset_3d_t source_offset = ignored_offset_3d;
        offset_3d_t dest_offset = ignored_offset_3d;
        image_subresource_t source_subresource = {};
        image_subresource_t dest_subresource = {};
        extent_3d_t extent = ignored_extent_3d;
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

        auto begin() const noexcept -> void;
        auto begin_render_pass(const framebuffer_t& framebuffer, const std::vector<clear_value_t>& clears) noexcept -> void;
        auto end_render_pass() noexcept -> void;
        auto copy_image(const image_copy_t& copy) const noexcept -> void;
        auto memory_barrier(const memory_barrier_t& barrier) const noexcept -> void;
        auto image_barrier(const image_memory_barrier_t& barrier) const noexcept -> void;

        auto end() const noexcept -> void;

    private:
        VkCommandBuffer _handle = {};

        struct {
            const framebuffer_t* framebuffer = nullptr;
        } _state;

        command_buffer_create_info_t _info = {};
        arc_ptr<const command_pool_t> _pool;
    };
}
