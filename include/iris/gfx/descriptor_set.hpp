#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/hash.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <array>
#include <span>
#include <vector>
#include <variant>

namespace ir {
    struct image_info_t {
        IR_NODISCARD constexpr auto operator ==(const image_info_t& other) const noexcept -> bool = default;

        VkSampler sampler = {};
        VkImageView view = {};
        image_layout_t layout = {};
    };

    struct buffer_info_t {
        IR_NODISCARD constexpr auto operator ==(const buffer_info_t& other) const noexcept -> bool = default;

        VkDeviceMemory memory = {};
        VkBuffer handle = {};
        uint64 offset = 0;
        uint64 size = whole_size;
        uint64 address = 0;
    };

    using descriptor_data = std::variant<image_info_t, buffer_info_t>;

    struct descriptor_content_t {
        IR_NODISCARD constexpr auto operator ==(const descriptor_content_t& other) const noexcept -> bool = default;

        uint32 binding = 0;
        descriptor_type_t type = {};
        std::vector<descriptor_data> contents;
    };

    struct descriptor_set_binding_t {
        IR_NODISCARD constexpr auto operator ==(const descriptor_set_binding_t& other) const noexcept -> bool = default;

        VkDescriptorPool pool = {};
        VkDescriptorSetLayout layout = {};
        std::vector<descriptor_content_t> bindings;
    };

    class descriptor_set_t : public enable_intrusive_refcount_t<descriptor_set_t> {
    public:
        using self = descriptor_set_t;
        using cache_key_type = descriptor_set_binding_t;
        using cache_value_type = arc_ptr<self>;

        constexpr static auto max_ttl = 8_u32;
        constexpr static auto is_persistent = false;

        descriptor_set_t(device_t& device) noexcept;
        ~descriptor_set_t() noexcept;

        IR_NODISCARD static auto make(
            device_t& device,
            const descriptor_layout_t& layout
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkDescriptorSet;
        IR_NODISCARD auto device() const noexcept -> device_t&;
        IR_NODISCARD auto pool() const noexcept -> const descriptor_pool_t&;
        IR_NODISCARD auto layout() const noexcept -> const descriptor_layout_t&;

    private:
        VkDescriptorSet _handle;

        std::reference_wrapper<device_t> _device;
        arc_ptr<const descriptor_pool_t> _pool;
        arc_ptr<const descriptor_layout_t> _layout;
    };

    class descriptor_set_builder_t {
    public:
        using self = descriptor_set_builder_t;

        descriptor_set_builder_t(pipeline_t& pipeline, uint32 set) noexcept;

        auto bind_uniform_buffer(uint32 binding, const buffer_info_t& buffer) noexcept -> self&;
        auto bind_storage_buffer(uint32 binding, const buffer_info_t& buffer) noexcept -> self&;
        auto bind_storage_image(uint32 binding, const image_view_t& view) noexcept -> self&;
        auto bind_texture(uint32 binding, const texture_t& texture) noexcept -> self&;
        auto bind_textures(uint32 binding, const std::vector<arc_ptr<texture_t>>& textures) noexcept -> self&;
        auto bind_sampled_image(
            uint32 binding,
            const image_view_t& view,
            image_layout_t layout = image_layout_t::e_shader_read_only_optimal
        ) noexcept -> self&;
        auto bind_combined_image_sampler(uint32 binding, const image_view_t& view, const sampler_t& sampler) noexcept -> self&;
        auto bind_combined_image_sampler(
            uint32 binding,
            std::span<std::reference_wrapper<const image_view_t>> views,
            const sampler_t& sampler
        ) noexcept -> self&;

        auto build() const noexcept -> arc_ptr<descriptor_set_t>;

    private:
        descriptor_set_binding_t _binding = {};

        std::reference_wrapper<const descriptor_layout_t> _layout;
    };
}

IR_MAKE_TRANSPARENT_EQUAL_TO_SPECIALIZATION(ir::descriptor_set_binding_t);
IR_MAKE_TRANSPARENT_EQUAL_TO_SPECIALIZATION(ir::descriptor_content_t);
IR_MAKE_TRANSPARENT_EQUAL_TO_SPECIALIZATION(ir::descriptor_data);
IR_MAKE_TRANSPARENT_EQUAL_TO_SPECIALIZATION(ir::image_info_t);
IR_MAKE_TRANSPARENT_EQUAL_TO_SPECIALIZATION(ir::buffer_info_t);

IR_MAKE_AVALANCHING_TRANSPARENT_HASH_SPECIALIZATION(ir::buffer_info_t, ([](const auto& buffer) {
    IR_PROFILE_SCOPED();
    auto seed = std::size_t();
    seed = ir::akl::hash<VkDeviceMemory>()(buffer.memory);
    seed = ir::akl::hash<VkBuffer>()(buffer.handle);
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::uint64>()(buffer.offset));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::uint64>()(buffer.size));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::uint64>()(buffer.address));
    return seed;
}));

IR_MAKE_AVALANCHING_TRANSPARENT_HASH_SPECIALIZATION(ir::image_info_t, ([](const auto& image) {
    IR_PROFILE_SCOPED();
    auto seed = std::size_t();
    seed = ir::akl::hash<VkSampler>()(image.sampler);
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::image_layout_t>()(image.layout));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<VkImageView>()(image.view));
    return seed;
}));

IR_MAKE_AVALANCHING_TRANSPARENT_HASH_SPECIALIZATION(ir::descriptor_content_t, ([](const auto& data) {
    IR_PROFILE_SCOPED();
    auto seed = std::size_t();
    seed = ir::akl::hash<ir::uint32>()(data.binding);
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<ir::descriptor_type_t>()(data.type));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<std::vector<ir::descriptor_data>>()(data.contents));
    return seed;
}));

IR_MAKE_AVALANCHING_TRANSPARENT_HASH_SPECIALIZATION(ir::descriptor_set_binding_t, ([](const auto& binding) {
    IR_PROFILE_SCOPED();
    auto seed = std::size_t();
    seed = ir::akl::hash<VkDescriptorPool>()(binding.pool);
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<VkDescriptorSetLayout>()(binding.layout));
    seed = ir::akl::wyhash::mix(seed, ir::akl::hash<std::vector<ir::descriptor_content_t>>()(binding.bindings));
    return seed;
}));
