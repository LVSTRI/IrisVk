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
#include <memory>

namespace ir {
    struct swapchain_create_info_t {
        image_usage_t usage =
            image_usage_t::e_color_attachment |
            image_usage_t::e_transfer_src;
        bool vsync = false;
        bool srgb = false;
    };

    class swapchain_t : public enable_intrusive_refcount_t<swapchain_t> {
    public:
        using self = swapchain_t;

        swapchain_t(const wsi_platform_t& wsi) noexcept;
        ~swapchain_t() noexcept;

        IR_NODISCARD static auto make(
            const device_t& device,
            const wsi_platform_t& wsi,
            const swapchain_create_info_t& info) noexcept -> intrusive_atomic_ptr_t<self>;

        IR_NODISCARD auto handle() const noexcept -> VkSwapchainKHR;
        IR_NODISCARD auto surface() const noexcept -> VkSurfaceKHR;
        IR_NODISCARD auto format() const noexcept -> resource_format_t;
        IR_NODISCARD auto width() const noexcept -> uint32;
        IR_NODISCARD auto height() const noexcept -> uint32;
        IR_NODISCARD auto images() const noexcept -> const std::vector<intrusive_atomic_ptr_t<image_t>>&;
        IR_NODISCARD auto image(uint32 index) const noexcept -> const image_t&;

        IR_NODISCARD auto info() const noexcept -> const swapchain_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;
        IR_NODISCARD auto wsi() const noexcept -> const wsi_platform_t&;

    private:
        VkSwapchainKHR _handle = {};
        VkSurfaceKHR _surface = {};
        resource_format_t _format = {};
        uint32 _width = 0;
        uint32 _height = 0;
        std::vector<intrusive_atomic_ptr_t<image_t>> _images;

        swapchain_create_info_t _info = {};
        std::reference_wrapper<const wsi_platform_t> _wsi;
        intrusive_atomic_ptr_t<const device_t> _device = {};
    };
}
