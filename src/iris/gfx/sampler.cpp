#include <iris/gfx/device.hpp>
#include <iris/gfx/sampler.hpp>

namespace ir {
    sampler_t::sampler_t(device_t& device) noexcept : _device(std::ref(device)) {
        IR_PROFILE_SCOPED();
    }

    sampler_t::~sampler_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroySampler(_device.get().handle(), _handle, nullptr);
        IR_LOG_INFO(_device.get().logger(), "destroyed sampler {}", fmt::ptr(_handle));
    }

    auto sampler_t::make(device_t& device, const sampler_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto& cache = device.cache<self>();
        if (cache.contains(info)) {
            return cache.acquire(info);
        }

        auto sampler = arc_ptr<self>(new self(device));
        auto sampler_reduction_info = VkSamplerReductionModeCreateInfo();
        sampler_reduction_info.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
        sampler_reduction_info.pNext = nullptr;
        if (info.reduction_mode) {
            sampler_reduction_info.reductionMode = as_enum_counterpart(*info.reduction_mode);
        }

        auto sampler_info = VkSamplerCreateInfo();
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        if (info.reduction_mode) {
            sampler_info.pNext = &sampler_reduction_info;
        }
        sampler_info.flags = {};
        sampler_info.magFilter = as_enum_counterpart(info.filter.mag);
        sampler_info.minFilter = as_enum_counterpart(info.filter.min);
        sampler_info.mipmapMode = as_enum_counterpart(info.mip_mode);
        sampler_info.addressModeU = as_enum_counterpart(info.address_mode.u);
        sampler_info.addressModeV = as_enum_counterpart(info.address_mode.v);
        sampler_info.addressModeW = as_enum_counterpart(info.address_mode.w);
        sampler_info.mipLodBias = info.lod_bias;
        sampler_info.anisotropyEnable = std::fpclassify(info.anisotropy) != FP_ZERO;
        sampler_info.maxAnisotropy = info.anisotropy;
        sampler_info.compareEnable = false;
        sampler_info.compareOp = {};
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 16.0f;
        sampler_info.borderColor = as_enum_counterpart(info.border_color);
        sampler_info.unnormalizedCoordinates = false;
        IR_VULKAN_CHECK(device.logger(), vkCreateSampler(device.handle(), &sampler_info, nullptr, &sampler->_handle));
        IR_LOG_INFO(
            device.logger(),
            "created sampler ({}): ({}, {}, {})",
            fmt::ptr(sampler->_handle),
            as_string(info.filter.mag),
            as_string(info.mip_mode),
            as_string(info.address_mode.u));
        sampler->_info = info;

        IR_LOG_WARN(device.logger(), "sampler_t ({}): cache miss", fmt::ptr(sampler->handle()));
        return cache.insert(info, std::move(sampler));
    }

    auto sampler_t::handle() const noexcept -> VkSampler {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto sampler_t::info() const noexcept -> const sampler_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto sampler_t::device() const noexcept -> device_t& {
        IR_PROFILE_SCOPED();
        return _device.get();
    }
}
