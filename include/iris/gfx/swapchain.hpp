#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <string>
#include <memory>
#include <span>

namespace ir {
    struct swapchain_create_info_t {
        std::string name = {};
        image_usage_t usage =
            image_usage_t::e_color_attachment |
            image_usage_t::e_transfer_dst;
        bool vsync = true;
        bool srgb = true;
    };

    class swapchain_t : public enable_intrusive_refcount_t<swapchain_t> {
    public:
        using self = swapchain_t;

        swapchain_t(const wsi_platform_t& wsi) noexcept;
        ~swapchain_t() noexcept;

        IR_NODISCARD static auto make(
            const device_t& device,
            const wsi_platform_t& wsi,
            const swapchain_create_info_t& info
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkSwapchainKHR;
        IR_NODISCARD auto surface() const noexcept -> VkSurfaceKHR;
        IR_NODISCARD auto format() const noexcept -> resource_format_t;
        IR_NODISCARD auto width() const noexcept -> uint32;
        IR_NODISCARD auto height() const noexcept -> uint32;
        IR_NODISCARD auto images() const noexcept -> std::span<const arc_ptr<image_t>>;
        IR_NODISCARD auto image(uint32 index) const noexcept -> const image_t&;

        IR_NODISCARD auto info() const noexcept -> const swapchain_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;
        IR_NODISCARD auto wsi() const noexcept -> const wsi_platform_t&;

        IR_NODISCARD auto acquire_next_image(const semaphore_t& semaphore) const noexcept -> std::pair<uint32, bool>;

    private:
        VkSwapchainKHR _handle = {};
        VkSurfaceKHR _surface = {};
        resource_format_t _format = {};
        uint32 _width = 0;
        uint32 _height = 0;
        std::vector<arc_ptr<image_t>> _images;

        swapchain_create_info_t _info = {};
        std::reference_wrapper<const wsi_platform_t> _wsi;
        arc_ptr<const device_t> _device = {};
    };
}
