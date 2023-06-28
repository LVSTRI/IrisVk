#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <optional>
#include <vector>
#include <span>

namespace ir {
    struct sampler_filter_combo_t {
        sampler_filter_t min = sampler_filter_t::e_nearest;
        sampler_filter_t mag = min;
    };

    struct sampler_address_mode_combo_t {
        sampler_address_mode_t u = sampler_address_mode_t::e_repeat;
        sampler_address_mode_t v = u;
        sampler_address_mode_t w = v;
    };

    struct sampler_create_info_t {
        sampler_filter_combo_t filter = {};
        sampler_mipmap_mode_t mip_mode = {};
        sampler_address_mode_combo_t address_mode = {};
        sampler_border_color_t border_color = {};
        std::optional<sampler_reduction_mode_t> reduction_mode = {};
        float32 lod_bias = 0.0f;
        float32 anisotropy = 0.0f;
    };

    class sampler_t : public enable_intrusive_refcount_t<sampler_t> {
    public:
        using self = sampler_t;

        sampler_t() noexcept;
        ~sampler_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device, const sampler_create_info_t& info) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkSampler;

        IR_NODISCARD auto info() const noexcept -> const sampler_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> arc_ptr<const device_t>;

    private:
        VkSampler _handle = {};

        sampler_create_info_t _info = {};
        arc_ptr<const device_t> _device = {};
    };
}
