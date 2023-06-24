#include <iris/gfx/device.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/swapchain.hpp>
#include <iris/gfx/render_pass.hpp>

namespace ir {
    IR_NODISCARD constexpr auto deduce_image_aspect(const image_create_info_t& info) noexcept -> image_aspect_t {
        switch (info.format) {
            case resource_format_t::e_s8_uint:
                return image_aspect_t::e_stencil;
            case resource_format_t::e_d16_unorm:
            case resource_format_t::e_x8_d24_unorm_pack32:
            case resource_format_t::e_d32_sfloat:
                return image_aspect_t::e_depth;
            case resource_format_t::e_d16_unorm_s8_uint:
            case resource_format_t::e_d24_unorm_s8_uint:
            case resource_format_t::e_d32_sfloat_s8_uint:
                return image_aspect_t::e_depth | image_aspect_t::e_stencil;
            default:
                return image_aspect_t::e_color;
        }
        IR_UNREACHABLE();
    }

    image_view_t::image_view_t(const image_t& image) noexcept : _image(std::cref(image)) {
        IR_PROFILE_SCOPED();
    }

    image_view_t::~image_view_t() noexcept {
        IR_PROFILE_SCOPED();
        IR_LOG_INFO(device().logger(), "image view {} destroyed", fmt::ptr(_handle));
        vkDestroyImageView(device().handle(), _handle, nullptr);
    }

    auto image_view_t::make(
        const device_t& device,
        const image_t& image,
        const image_view_create_info_t& info
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto image_view = arc_ptr<self>(new self(image));
        const auto aspect = deduce_image_aspect(image.info());
        auto image_view_info = VkImageViewCreateInfo();
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.pNext = nullptr;
        image_view_info.flags = {};
        image_view_info.image = image.handle();
        // TODO: don't hardcode
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.format = as_enum_counterpart(
            info.format == resource_format_t::e_undefined ?
                image.format() :
                info.format);
        image_view_info.components.r = as_enum_counterpart(info.swizzle.r);
        image_view_info.components.g = as_enum_counterpart(info.swizzle.g);
        image_view_info.components.b = as_enum_counterpart(info.swizzle.b);
        image_view_info.components.a = as_enum_counterpart(info.swizzle.a);
        image_view_info.subresourceRange.aspectMask = as_enum_counterpart(aspect);
         if (info.subresource.level == level_ignored) {
            image_view_info.subresourceRange.baseMipLevel = 0;
            image_view_info.subresourceRange.levelCount = image.levels();
        } else {
            image_view_info.subresourceRange.baseMipLevel = info.subresource.level;
            image_view_info.subresourceRange.levelCount = info.subresource.level_count;
        }
        if (info.subresource.layer == layer_ignored) {
            image_view_info.subresourceRange.baseArrayLayer = 0;
            image_view_info.subresourceRange.layerCount = image.layers();
        } else {
            image_view_info.subresourceRange.baseArrayLayer = info.subresource.layer;
            image_view_info.subresourceRange.layerCount = info.subresource.layer_count;
        }
        IR_VULKAN_CHECK(device.logger(), vkCreateImageView(device.handle(), &image_view_info, nullptr, &image_view->_handle));
        IR_LOG_INFO(device.logger(), "image view {} for image {} created", fmt::ptr(image_view->_handle), fmt::ptr(image.handle()));

        image_view->_aspect = aspect;
        image_view->_info = info;
        image_view->_device = device.as_intrusive_ptr();
        return image_view;
    }

    auto image_view_t::handle() const noexcept -> VkImageView {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto image_view_t::aspect() const noexcept -> image_aspect_t {
        IR_PROFILE_SCOPED();
        return _aspect;
    }

    auto image_view_t::info() const noexcept -> const image_view_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto image_view_t::image() const noexcept -> const image_t& {
        IR_PROFILE_SCOPED();
        return _image.get();
    }

    auto image_view_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }

    image_t::image_t() noexcept = default;

    image_t::~image_t() noexcept {
        IR_PROFILE_SCOPED();
        if (_view) {
            _view.reset();
        }
        if (_allocation) {
            vmaDestroyImage(device().allocator(), _handle, _allocation);
        }
        IR_LOG_INFO(device().logger(), "image {} destroyed", fmt::ptr(_handle));
    }

    auto image_t::make(
        const device_t& device,
        const image_create_info_t& info
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto image = arc_ptr<self>(new self());
        const auto family = [&]() {
            switch (info.queue) {
                case queue_type_t::e_graphics: return device.graphics_queue().family();
                case queue_type_t::e_compute: return device.compute_queue().family();
                case queue_type_t::e_transfer: return device.transfer_queue().family();
            }
            IR_UNREACHABLE();
        }();

        auto image_info = VkImageCreateInfo();
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext = nullptr;
        image_info.flags = {};
        // TODO: don't hardcode
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = as_enum_counterpart(info.format);
        image_info.extent = { info.width, info.height, 1 };
        image_info.mipLevels = info.levels;
        image_info.arrayLayers = info.layers;
        image_info.samples = as_enum_counterpart(info.samples);
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = as_enum_counterpart(info.usage);
        // TODO: don't hardcode
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.queueFamilyIndexCount = 1;
        image_info.pQueueFamilyIndices = &family;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        auto allocation_info = VmaAllocationCreateInfo();
        allocation_info.flags = {};
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocation_info.requiredFlags = {};
        allocation_info.preferredFlags = {};
        allocation_info.memoryTypeBits = {};
        allocation_info.pool = {};
        allocation_info.pUserData = nullptr;
        allocation_info.priority = 1.0f;
        IR_VULKAN_CHECK(
            device.logger(),
            vmaCreateImage(
                device.allocator(),
                &image_info,
                &allocation_info,
                &image->_handle,
                &image->_allocation,
                nullptr));
        image->_info = info;
        if (info.view) {
            image->_view = image_view_t::make(
                device,
                *image,
                *info.view);
        }

        image->_device = device.as_intrusive_ptr();
        return image;
    }

    auto image_t::make_from_swapchain(
        const device_t& device,
        const swapchain_t& swapchain,
        const image_create_info_t& info
    ) noexcept -> std::vector<arc_ptr<self>> {
        IR_PROFILE_SCOPED();
        auto count = 0_u32;
        IR_VULKAN_CHECK(
            device.logger(),
            vkGetSwapchainImagesKHR(
                device.handle(),
                swapchain.handle(),
                &count,
                nullptr));
        auto swapchain_images = std::vector<VkImage>(count);
        IR_VULKAN_CHECK(
            device.logger(),
            vkGetSwapchainImagesKHR(
                device.handle(),
                swapchain.handle(),
                &count,
                swapchain_images.data()));
        IR_LOG_INFO(device.logger(), "swapchain images initialized");
        auto images = std::vector<arc_ptr<self>>();
        for (auto i = 0_u32; i < count; ++i) {
            auto image = arc_ptr<self>(new self());
            image->_handle = swapchain_images[i];
            image->_allocation = VmaAllocation();
            image->_info = info;
            image->_device = device.as_intrusive_ptr();
            if (info.view) {
                image->_view = image_view_t::make(
                    device,
                    *image,
                    *info.view);
            }
            images.emplace_back(std::move(image));
        }
        return images;
    }

    auto image_t::make_from_attachment(
        const device_t& device,
        const attachment_info_t& attachment,
        const image_create_info_t& info
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        return image_t::make(device, {
            .width = info.width,
            .height = info.height,
            .levels = info.levels,
            .layers = info.layers,
            .queue = info.queue,
            .samples = attachment.samples,
            .usage = info.usage,
            .format = attachment.format,
            .layout = info.layout,
            .view = info.view
        });
    }

    auto image_t::handle() const noexcept -> VkImage {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto image_t::allocation() const noexcept -> VmaAllocation {
        IR_PROFILE_SCOPED();
        return _allocation;
    }

    auto image_t::view() const noexcept -> const image_view_t& {
        IR_PROFILE_SCOPED();
        return *_view;
    }

    auto image_t::width() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _info.width;
    }

    auto image_t::height() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _info.height;
    }

    auto image_t::levels() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _info.levels;
    }

    auto image_t::layers() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _info.layers;
    }

    auto image_t::samples() const noexcept -> sample_count_t {
        IR_PROFILE_SCOPED();
        return _info.samples;
    }

    auto image_t::usage() const noexcept -> image_usage_t {
        IR_PROFILE_SCOPED();
        return _info.usage;
    }

    auto image_t::format() const noexcept -> resource_format_t {
        IR_PROFILE_SCOPED();
        return _info.format;
    }

    auto image_t::layout() const noexcept -> image_layout_t {
        IR_PROFILE_SCOPED();
        return _info.layout;
    }

    auto image_t::info() const noexcept -> const image_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto image_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }
}
