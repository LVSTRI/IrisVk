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
        image_view_info.viewType = image.layers() == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
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
        IR_VULKAN_CHECK(image.device().logger(), vkCreateImageView(image.device().handle(), &image_view_info, nullptr, &image_view->_handle));
        IR_LOG_INFO(image.device().logger(), "image view {} for image {} created", fmt::ptr(image_view->_handle), fmt::ptr(image.handle()));

        image_view->_aspect = aspect;
        image_view->_info = info;
        image_view->_device = image.device().as_intrusive_ptr();
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
        } else if (is_sparsely_bound()) {
            vkDestroyImage(device().handle(), _handle, nullptr);
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
        auto is_sparse = false;
        auto image_info = VkImageCreateInfo();
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext = nullptr;
        image_info.flags = {};
        if ((info.flags & image_flag_t::e_sparse_binding) == image_flag_t::e_sparse_binding) {
            is_sparse = true;
            image_info.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT;
            if ((info.flags & image_flag_t::e_sparse_residency) == image_flag_t::e_sparse_residency) {
                image_info.flags |= VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
            }
        }
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

        auto format_properties = VkFormatProperties();
        vkGetPhysicalDeviceFormatProperties(device.gpu(), image_info.format, &format_properties);

        if (!is_sparse) {
            auto allocation_info = VmaAllocationCreateInfo();
            allocation_info.flags = {};
            allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
            allocation_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            allocation_info.preferredFlags = 0;
            allocation_info.memoryTypeBits = 0;
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
            vkGetImageMemoryRequirements(device.handle(), image->_handle, &image->_requirements);
        } else {
            IR_VULKAN_CHECK(device.logger(), vkCreateImage(device.handle(), &image_info, nullptr, &image->_handle));
            auto memory_requirements = VkMemoryRequirements();
            vkGetImageMemoryRequirements(device.handle(), image->_handle, &memory_requirements);
            auto sparse_req_count = 0_u32;
            vkGetImageSparseMemoryRequirements(device.handle(), image->_handle, &sparse_req_count, nullptr);
            auto sparse_reqs = std::vector<VkSparseImageMemoryRequirements>(sparse_req_count);
            vkGetImageSparseMemoryRequirements(device.handle(), image->_handle, &sparse_req_count, sparse_reqs.data());
            auto image_sparse_req = VkSparseImageMemoryRequirements();
            const auto aspect = as_enum_counterpart(deduce_image_aspect(info));
            for (const auto& requirement : sparse_reqs) {
                if ((requirement.formatProperties.aspectMask & aspect) == aspect) {
                    IR_LOG_INFO(
                        device.logger(), "image {} sparse info | granularity: {}x{}x{}, tail first LOD: {}, tail size: {}",
                        fmt::ptr(image->_handle),
                        requirement.formatProperties.imageGranularity.width,
                        requirement.formatProperties.imageGranularity.height,
                        requirement.formatProperties.imageGranularity.depth,
                        requirement.imageMipTailFirstLod,
                        requirement.imageMipTailSize);
                    image_sparse_req = requirement;
                    break;
                }
            }
            image->_requirements = memory_requirements;
            image->_sparse_info = image_sparse_req;
        }

        image->_info = info;
        image->_device = device.as_intrusive_ptr();
        if (info.view) {
            image->_view = image_view_t::make(*image, *info.view);
        }
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

    auto image_t::memory_requirements() const noexcept -> const VkMemoryRequirements& {
        IR_PROFILE_SCOPED();
        return _requirements;
    }

    auto image_t::sparse_requirements() const noexcept -> const VkSparseImageMemoryRequirements& {
        IR_PROFILE_SCOPED();
        return _sparse_info;
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

    auto image_t::granularity() const noexcept -> extent_3d_t {
        IR_PROFILE_SCOPED();
        return {
            _sparse_info.formatProperties.imageGranularity.width,
            _sparse_info.formatProperties.imageGranularity.height,
            _sparse_info.formatProperties.imageGranularity.depth
        };
    }

    auto image_t::samples() const noexcept -> sample_count_t {
        IR_PROFILE_SCOPED();
        return _info.samples;
    }

    auto image_t::usage() const noexcept -> image_usage_t {
        IR_PROFILE_SCOPED();
        return _info.usage;
    }

    auto image_t::is_sparsely_bound() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return (_info.flags & image_flag_t::e_sparse_binding) == image_flag_t::e_sparse_binding;
    }

    auto image_t::is_sparsely_resident() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return (_info.flags & image_flag_t::e_sparse_residency) == image_flag_t::e_sparse_residency;
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
