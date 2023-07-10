#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/hash.hpp>
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

        constexpr auto operator ==(const sampler_filter_combo_t&) const noexcept -> bool = default;
    };

    struct sampler_address_mode_combo_t {
        sampler_address_mode_t u = sampler_address_mode_t::e_repeat;
        sampler_address_mode_t v = u;
        sampler_address_mode_t w = v;

        constexpr auto operator ==(const sampler_address_mode_combo_t&) const noexcept -> bool = default;
    };

    struct sampler_create_info_t {
        sampler_filter_combo_t filter = {};
        sampler_mipmap_mode_t mip_mode = {};
        sampler_address_mode_combo_t address_mode = {};
        sampler_border_color_t border_color = {};
        std::optional<sampler_reduction_mode_t> reduction_mode = {};
        float32 lod_bias = 0.0f;
        float32 anisotropy = 0.0f;

        constexpr auto operator ==(const sampler_create_info_t&) const noexcept -> bool = default;
    };

    class sampler_t : public enable_intrusive_refcount_t<sampler_t> {
    public:
        using self = sampler_t;
        using cache_key_type = sampler_create_info_t;
        using cache_value_type = arc_ptr<self>;

        constexpr static auto max_ttl = -1_u32;
        constexpr static auto is_persistent = true;

        sampler_t(device_t& device) noexcept;
        ~sampler_t() noexcept;

        IR_NODISCARD static auto make(device_t& device, const sampler_create_info_t& info) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkSampler;

        IR_NODISCARD auto info() const noexcept -> const sampler_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> device_t&;

    private:
        VkSampler _handle = {};

        sampler_create_info_t _info = {};
        std::reference_wrapper<device_t> _device;
    };
}

IR_MAKE_TRANSPARENT_EQUAL_TO_SPECIALIZATION(ir::sampler_filter_combo_t);
IR_MAKE_TRANSPARENT_EQUAL_TO_SPECIALIZATION(ir::sampler_address_mode_combo_t);
IR_MAKE_TRANSPARENT_EQUAL_TO_SPECIALIZATION(ir::sampler_create_info_t);

IR_MAKE_AVALANCHING_TRANSPARENT_HASH_SPECIALIZATION(ir::sampler_filter_combo_t, ([](const auto& x) -> std::size_t {
    IR_PROFILE_SCOPED();
    auto seed = std::size_t();
    seed = ir::akl::hash<ir::sampler_filter_t>()(x.min);
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::sampler_filter_t>()(x.mag));
    return seed;
}));

IR_MAKE_AVALANCHING_TRANSPARENT_HASH_SPECIALIZATION(ir::sampler_address_mode_combo_t, ([](const auto& x) -> std::size_t {
    IR_PROFILE_SCOPED();
    auto seed = std::size_t();
    seed = ir::akl::hash<ir::sampler_address_mode_t>()(x.u);
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::sampler_address_mode_t>()(x.v));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::sampler_address_mode_t>()(x.w));
    return seed;
}));

IR_MAKE_AVALANCHING_TRANSPARENT_HASH_SPECIALIZATION(ir::sampler_create_info_t, ([](const auto& x) -> std::size_t {
    IR_PROFILE_SCOPED();
    auto seed = std::size_t();
    seed = ir::akl::hash<ir::sampler_filter_combo_t>()(x.filter);
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::sampler_mipmap_mode_t>()(x.mip_mode));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::sampler_address_mode_combo_t>()(x.address_mode));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::sampler_border_color_t>()(x.border_color));
    if (x.reduction_mode) {
        seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::sampler_reduction_mode_t>()(*x.reduction_mode));
    }
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::float32>()(x.lod_bias));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::float32>()(x.anisotropy));
    return seed;
}));
