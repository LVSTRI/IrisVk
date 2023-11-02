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

#include <optional>
#include <vector>
#include <string>
#include <memory>

namespace ir {
    struct image_subresource_t {
        uint32 level = level_ignored;
        uint32 level_count = remaining_levels;
        uint32 layer = layer_ignored;
        uint32 layer_count = remaining_layers;
    };

    struct image_view_create_info_t {
        std::string name = {};
        resource_format_t format = resource_format_t::e_undefined;
        struct {
            component_swizzle_t r = component_swizzle_t::e_identity;
            component_swizzle_t g = component_swizzle_t::e_identity;
            component_swizzle_t b = component_swizzle_t::e_identity;
            component_swizzle_t a = component_swizzle_t::e_identity;
        } swizzle = {};
        image_subresource_t subresource = {};
    };

    constexpr static auto default_image_view_info = image_view_create_info_t();

    enum class image_flag_t {
        e_sparse_binding = 1 << 0,
        e_sparse_residency = 1 << 1,
    };

    struct image_create_info_t {
        std::string name = {};
        uint32 width = 0;
        uint32 height = 0;
        uint32 levels = 1;
        uint32 layers = 1;
        queue_type_t queue = queue_type_t::e_graphics;
        sample_count_t samples = sample_count_t::e_1;
        image_usage_t usage = {};
        image_flag_t flags = {};
        resource_format_t format = resource_format_t::e_undefined;
        image_layout_t layout = image_layout_t::e_undefined;
        std::optional<image_view_create_info_t> view = std::nullopt;
    };

    class image_view_t : public enable_intrusive_refcount_t<image_view_t> {
    public:
        using self = image_view_t;

        image_view_t(const image_t& image) noexcept;
        ~image_view_t() noexcept;

        IR_NODISCARD static auto make(
            const image_t& image,
            const image_view_create_info_t& info
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkImageView;
        IR_NODISCARD auto aspect() const noexcept -> image_aspect_t;
        IR_NODISCARD auto info() const noexcept -> const image_view_create_info_t&;
        IR_NODISCARD auto image() const noexcept -> const image_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;

    private:
        VkImageView _handle = {};
        image_aspect_t _aspect = {};

        image_view_create_info_t _info = {};
        std::reference_wrapper<const image_t> _image;
        arc_ptr<const device_t> _device = {};
    };

    class image_t : public enable_intrusive_refcount_t<image_t> {
    public:
        using self = image_t;

        image_t() noexcept;
        ~image_t() noexcept;

        IR_NODISCARD static auto make(
            const device_t& device,
            const image_create_info_t& info
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD static auto make_from_swapchain(
            const device_t& device,
            const swapchain_t& swapchain,
            const image_create_info_t& info
        ) noexcept -> std::vector<arc_ptr<self>>;

        IR_NODISCARD static auto make_from_attachment(
            const device_t& device,
            const attachment_info_t& attachment,
            const image_create_info_t& info
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkImage;
        IR_NODISCARD auto memory_requirements() const noexcept -> const VkMemoryRequirements&;
        IR_NODISCARD auto sparse_requirements() const noexcept -> const VkSparseImageMemoryRequirements&;
        IR_NODISCARD auto allocation() const noexcept -> VmaAllocation;
        IR_NODISCARD auto view() const noexcept -> const image_view_t&;

        IR_NODISCARD auto width() const noexcept -> uint32;
        IR_NODISCARD auto height() const noexcept -> uint32;
        IR_NODISCARD auto levels() const noexcept -> uint32;
        IR_NODISCARD auto layers() const noexcept -> uint32;
        IR_NODISCARD auto granularity() const noexcept -> extent_3d_t;
        IR_NODISCARD auto samples() const noexcept -> sample_count_t;
        IR_NODISCARD auto usage() const noexcept -> image_usage_t;
        IR_NODISCARD auto is_sparsely_bound() const noexcept -> bool;
        IR_NODISCARD auto is_sparsely_resident() const noexcept -> bool;
        IR_NODISCARD auto format() const noexcept -> resource_format_t;
        IR_NODISCARD auto layout() const noexcept -> image_layout_t;

        IR_NODISCARD auto info() const noexcept -> const image_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;

    private:
        VkImage _handle = {};
        VkMemoryRequirements _requirements = {};
        VkSparseImageMemoryRequirements _sparse_info = {};
        VmaAllocation _allocation = {};
        arc_ptr<image_view_t> _view;

        image_create_info_t _info = {};
        arc_ptr<const device_t> _device = {};
    };

    class virtual_image_t : enable_intrusive_refcount_t<virtual_image_t> {
    public:
        using self = virtual_image_t;

        virtual_image_t() noexcept;
        ~virtual_image_t() noexcept;

    private:

        arc_ptr<image_t> _handle;
    };
}
