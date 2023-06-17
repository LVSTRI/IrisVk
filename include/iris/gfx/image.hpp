#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <iris/gfx/queue.hpp>

#include <volk.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <memory>

namespace ir {
    struct image_create_info_t {
        uint32 width = 0;
        uint32 height = 0;
        uint32 mips = 1;
        uint32 layers = 1;
        queue_type_t queue = queue_type_t::e_graphics;
        image_sample_count_t samples = image_sample_count_t::e_1;
        image_usage_t usage = {};
        resource_format_t format = resource_format_t::e_undefined;
        image_layout_t layout = image_layout_t::e_undefined;
    };

    class image_t : public enable_intrusive_refcount_t {
    public:
        using self = image_t;

        image_t() noexcept;
        ~image_t() noexcept;

        IR_NODISCARD static auto make(
            intrusive_atomic_ptr_t<device_t> device,
            const image_create_info_t& info
        ) noexcept -> intrusive_atomic_ptr_t<self>;

        IR_NODISCARD auto handle() const noexcept -> VkImage;
        IR_NODISCARD auto allocation() const noexcept -> VmaAllocation;
        IR_NODISCARD auto aspect() const noexcept -> image_aspect_t;

        IR_NODISCARD auto width() const noexcept -> uint32;
        IR_NODISCARD auto height() const noexcept -> uint32;
        IR_NODISCARD auto mips() const noexcept -> uint32;
        IR_NODISCARD auto layers() const noexcept -> uint32;
        IR_NODISCARD auto samples() const noexcept -> image_sample_count_t;
        IR_NODISCARD auto usage() const noexcept -> image_usage_t;
        IR_NODISCARD auto format() const noexcept -> resource_format_t;
        IR_NODISCARD auto layout() const noexcept -> image_layout_t;

        IR_NODISCARD auto info() const noexcept -> const image_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;

    private:
        VkImage _handle = {};
        VmaAllocation _allocation = {};
        image_aspect_t _aspect = {};

        image_create_info_t _info = {};
        intrusive_atomic_ptr_t<device_t> _device = {};
    };
}
