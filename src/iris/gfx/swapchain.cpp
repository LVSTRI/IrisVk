#include <iris/gfx/device.hpp>
#include <iris/gfx/instance.hpp>
#include <iris/gfx/queue.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/swapchain.hpp>

#include <iris/wsi/wsi_platform.hpp>

namespace ir {
    swapchain_t::swapchain_t(const wsi_platform_t& wsi) noexcept : _wsi(std::cref(wsi)) {
        IR_PROFILE_SCOPED();
    }

    swapchain_t::~swapchain_t() noexcept {
        IR_PROFILE_SCOPED();
        _images.clear();
        vkDestroySurfaceKHR(_device.as_const_ref().instance().handle(), _surface, nullptr);
        vkDestroySwapchainKHR(_device.as_const_ref().handle(), _handle, nullptr);
        IR_LOG_INFO(_device.as_const_ref().logger(), "swapchain destroyed");
    }

    auto swapchain_t::make(
        const device_t& device,
        const wsi_platform_t& wsi,
        const swapchain_create_info_t& info
    ) noexcept -> intrusive_atomic_ptr_t<self> {
        IR_PROFILE_SCOPED();
        auto swapchain = intrusive_atomic_ptr_t(new self(wsi));

        auto surface = reinterpret_cast<VkSurfaceKHR>(
            wsi.make_surface(reinterpret_cast<VkInstance>(device.instance().handle())));
        IR_LOG_INFO(device.logger(), "wsi surface initialized");

        const auto family = device.graphics_queue().family();
        auto is_present_supported = VkBool32();
        IR_VULKAN_CHECK(
            device.logger(),
            vkGetPhysicalDeviceSurfaceSupportKHR(device.gpu(), family, surface, &is_present_supported));
        IR_ASSERT(is_present_supported, "graphics queue does not support presentation");

        auto capabilities = VkSurfaceCapabilitiesKHR();
        IR_VULKAN_CHECK(
            device.logger(),
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.gpu(), surface, &capabilities));
        IR_LOG_INFO(
            device.logger(),
            "wsi surface capabilities: {{ "
            "min_image_count = {}, "
            "max_image_count = {}, "
            "current_extent = {{ {}, {} }} "
            "}}",
            capabilities.minImageCount,
            capabilities.maxImageCount,
            capabilities.currentExtent.width,
            capabilities.currentExtent.height);

        auto format_count = 0_u32;
        IR_VULKAN_CHECK(
            device.logger(),
            vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpu(), surface, &format_count, nullptr));
        auto surface_formats = std::vector<VkSurfaceFormatKHR>(format_count);
        IR_VULKAN_CHECK(
            device.logger(),
            vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpu(), surface, &format_count, surface_formats.data()));
        for (const auto& surface_format : surface_formats) {
            IR_LOG_INFO(
                device.logger(),
                "wsi surface format: {{ "
                "format = {}, "
                "color_space = {} "
                "}}",
                as_string(surface_format.format),
                as_string(surface_format.colorSpace));
        }

        auto present_mode_count = 0_u32;
        IR_VULKAN_CHECK(
            device.logger(),
            vkGetPhysicalDeviceSurfacePresentModesKHR(device.gpu(), surface, &present_mode_count, nullptr));
        auto present_modes = std::vector<VkPresentModeKHR>(present_mode_count);
        IR_VULKAN_CHECK(
            device.logger(),
            vkGetPhysicalDeviceSurfacePresentModesKHR(device.gpu(), surface, &present_mode_count, present_modes.data()));

        auto swapchain_image_count = 3_u32;
        const auto min_image_count = capabilities.minImageCount ? capabilities.minImageCount : 1_u32;
        const auto max_image_count = capabilities.maxImageCount ? capabilities.maxImageCount : swapchain_image_count;
        swapchain_image_count = std::clamp(swapchain_image_count, min_image_count, max_image_count);
        IR_LOG_INFO(device.logger(), "swapchain image count: {}", swapchain_image_count);

        if (capabilities.currentExtent.width != -1_u32) {
            if (capabilities.currentExtent.width == 0) {
                IR_VULKAN_CHECK(
                    device.logger(),
                    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.gpu(), swapchain->_surface, &capabilities));
            }
            swapchain->_width = capabilities.currentExtent.width;
            swapchain->_height = capabilities.currentExtent.height;
        } else {
            swapchain->_width = std::clamp(wsi.width(), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            swapchain->_height = std::clamp(wsi.height(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }
        IR_LOG_INFO(device.logger(), "swapchain extent: {{ {}, {} }}", swapchain->_width, swapchain->_height);

        auto format = info.srgb ?
            resource_format_t::e_b8g8r8a8_srgb :
            resource_format_t::e_b8g8r8a8_unorm;
        auto color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        IR_ASSERT(std::ranges::any_of(surface_formats, [format, color_space](const auto& each) {
            return
                each.format == as_enum_counterpart(format) &&
                each.colorSpace == color_space;
        }), "requested format not supported");

        auto present_mode = info.vsync ?
            VK_PRESENT_MODE_FIFO_KHR :
            VK_PRESENT_MODE_MAILBOX_KHR;
        IR_ASSERT(std::ranges::any_of(present_modes, [present_mode](const auto& each) {
            return each == present_mode;
        }), "requested present mode not supported");

        auto swapchain_info = VkSwapchainCreateInfoKHR();
        swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_info.pNext = nullptr;
        swapchain_info.flags = {};
        swapchain_info.surface = surface;
        swapchain_info.minImageCount = swapchain_image_count;
        swapchain_info.imageFormat = as_enum_counterpart(format);
        swapchain_info.imageColorSpace = color_space;
        swapchain_info.imageExtent = { swapchain->_width, swapchain->_height };
        swapchain_info.imageArrayLayers = 1;
        swapchain_info.imageUsage = as_enum_counterpart(info.usage);
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_info.queueFamilyIndexCount = 1;
        swapchain_info.pQueueFamilyIndices = &family;
        swapchain_info.preTransform = capabilities.currentTransform;
        swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.presentMode = present_mode;
        swapchain_info.clipped = true;
        swapchain_info.oldSwapchain = nullptr;
        IR_VULKAN_CHECK(
            device.logger(),
            vkCreateSwapchainKHR(device.handle(), &swapchain_info, nullptr, &swapchain->_handle));
        IR_LOG_INFO(device.logger(), "swapchain initialized");

        auto images = image_t::make_from_swapchain(device, *swapchain, {
            .width = swapchain->_width,
            .height = swapchain->_height,
            .layers = swapchain_info.imageArrayLayers,
            .queue = queue_type_t::e_graphics,
            .usage = info.usage,
            .format = format,
            .view = { {
                .swizzle = {
                    .r = component_swizzle_t::e_identity,
                    .g = component_swizzle_t::e_identity,
                    .b = component_swizzle_t::e_identity,
                    .a = component_swizzle_t::e_identity
                }
            } }
        });
        swapchain->_images = std::move(images);
        swapchain->_info = info;
        swapchain->_device = device.as_intrusive_ptr();
        return swapchain;
    }

    auto swapchain_t::handle() const noexcept -> VkSwapchainKHR {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto swapchain_t::surface() const noexcept -> VkSurfaceKHR {
        IR_PROFILE_SCOPED();
        return _surface;
    }

    auto swapchain_t::format() const noexcept -> resource_format_t {
        IR_PROFILE_SCOPED();
        return _format;
    }

    auto swapchain_t::width() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _width;
    }

    auto swapchain_t::height() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _height;
    }

    auto swapchain_t::images() const noexcept -> const std::vector<intrusive_atomic_ptr_t<image_t>>& {
        IR_PROFILE_SCOPED();
        return _images;
    }

    auto swapchain_t::image(uint32 index) const noexcept -> const image_t& {
        IR_PROFILE_SCOPED();
        return *_images[index];
    }

    auto swapchain_t::info() const noexcept -> const swapchain_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto swapchain_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }

    auto swapchain_t::wsi() const noexcept -> const wsi_platform_t& {
        IR_PROFILE_SCOPED();
        return _wsi.get();
    }
}
