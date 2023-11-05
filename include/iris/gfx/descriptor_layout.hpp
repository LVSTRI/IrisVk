#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <array>
#include <string>
#include <span>
#include <vector>

namespace ir {
    struct descriptor_binding_t {
        uint32 set = 0;
        uint32 binding = 0;
        uint32 count = 0;
        descriptor_type_t type = {};
        shader_stage_t stage = {};
        descriptor_binding_flag_t flags = {};
        bool is_dynamic = false;

        IR_NODISCARD constexpr auto operator ==(const descriptor_binding_t& other) const noexcept -> bool = default;
    };

    struct descriptor_layout_create_info_t {
        std::string name = {};
        std::span<const descriptor_binding_t> bindings;
    };

    class descriptor_layout_t : public enable_intrusive_refcount_t<descriptor_layout_t> {
    public:
        using self = descriptor_layout_t;
        using cache_key_type = std::vector<descriptor_binding_t>;
        using cache_value_type = arc_ptr<self>;

        constexpr static auto max_ttl = -1_u32;
        constexpr static auto is_persistent = true;

        descriptor_layout_t(device_t& device) noexcept;
        ~descriptor_layout_t() noexcept;

        IR_NODISCARD static auto make(device_t& device, const descriptor_layout_create_info_t& info) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkDescriptorSetLayout;
        IR_NODISCARD auto device() const noexcept -> device_t&;

        IR_NODISCARD auto bindings() const noexcept -> std::span<const descriptor_binding_t>;
        IR_NODISCARD auto binding(uint32 index) const noexcept -> const descriptor_binding_t&;
        IR_NODISCARD auto index() const noexcept -> uint32;
        IR_NODISCARD auto is_dynamic() const noexcept -> bool;

    private:
        VkDescriptorSetLayout _handle = {};

        std::vector<descriptor_binding_t> _bindings;
        std::reference_wrapper<device_t> _device;
    };

    IR_NODISCARD constexpr auto make_descriptor_reference(uint32 set, uint32 binding) noexcept -> descriptor_reference {
        return static_cast<descriptor_reference>(set) << 32 | binding;
    }

    IR_NODISCARD constexpr auto unpack_descriptor_reference(descriptor_reference reference) noexcept -> std::pair<uint32, uint32> {
        return std::make_pair(reference >> 32, reference & 0xffffffff);
    }
}

IR_MAKE_AVALANCHING_TRANSPARENT_HASH_SPECIALIZATION(ir::descriptor_binding_t, ([](const auto& x) -> std::size_t {
    IR_PROFILE_SCOPED();
    auto seed = std::size_t();
    seed = ir::akl::hash<ir::uint32>()(x.set);
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::uint32>()(x.binding));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::uint32>()(x.count));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::descriptor_type_t>()(x.type));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::shader_stage_t>()(x.stage));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::descriptor_binding_flag_t>()(x.flags));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<bool>()(x.is_dynamic));
    return seed;
}));
IR_MAKE_TRANSPARENT_EQUAL_TO_SPECIALIZATION(ir::descriptor_binding_t);