#include <iris/gfx/device.hpp>
#include <iris/gfx/image.hpp>

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

    image_t::image_t() noexcept = default;

    image_t::~image_t() noexcept = default;

    auto image_t::make(
        intrusive_atomic_ptr_t<device_t> device,
        const image_create_info_t& info
    ) noexcept -> intrusive_atomic_ptr_t<self> {
        auto image = intrusive_atomic_ptr_t(new self());
        const auto family = [&]() {
            switch (info.queue) {
                case queue_type_t::e_graphics: return device.as_const_ref().graphics_queue().family();
                case queue_type_t::e_compute: return device.as_const_ref().compute_queue().family();
                case queue_type_t::e_transfer: return device.as_const_ref().transfer_queue().family();
            }
            IR_UNREACHABLE();
        }();

        auto image_info = VkImageCreateInfo();
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext = nullptr;
        image_info.flags = {};
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = as_enum_counterpart(info.format);
        image_info.extent = { info.width, info.height, 1 };
        image_info.mipLevels = info.mips;
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
            device.as_const_ref().logger(),
            vmaCreateImage(
                device.as_const_ref().allocator(),
                &image_info,
                &allocation_info,
                &image->_handle,
                &image->_allocation,
                nullptr));

        image->_info = info;
        image->_device = std::move(device);
        return image;
    }

    auto image_t::handle() const noexcept -> VkImage {
        return _handle;
    }

    auto image_t::allocation() const noexcept -> VmaAllocation {
        return _allocation;
    }

    auto image_t::aspect() const noexcept -> image_aspect_t {
        return _aspect;
    }

    auto image_t::width() const noexcept -> uint32 {
        return _info.width;
    }

    auto image_t::height() const noexcept -> uint32 {
        return _info.height;
    }

    auto image_t::mips() const noexcept -> uint32 {
        return _info.mips;
    }

    auto image_t::layers() const noexcept -> uint32 {
        return _info.layers;
    }

    auto image_t::samples() const noexcept -> image_sample_count_t {
        return _info.samples;
    }

    auto image_t::usage() const noexcept -> image_usage_t {
        return _info.usage;
    }

    auto image_t::format() const noexcept -> resource_format_t {
        return _info.format;
    }

    auto image_t::layout() const noexcept -> image_layout_t {
        return _info.layout;
    }

    auto image_t::info() const noexcept -> const image_create_info_t& {
        return _info;
    }

    auto image_t::device() const noexcept -> const device_t& {
        return *_device;
    }
}
