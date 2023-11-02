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
#include <span>
#include <string>
#include <vector>

namespace ir {
    class descriptor_pool_t : public enable_intrusive_refcount_t<descriptor_pool_t> {
    public:
        using self = descriptor_pool_t;
        using descriptor_size_table = akl::fast_hash_map<descriptor_type_t, uint32>;

        constexpr static auto max_ttl = 16_u32;

        descriptor_pool_t(device_t& device) noexcept;
        ~descriptor_pool_t() noexcept;

        IR_NODISCARD static auto make(
            device_t& device,
            uint32 initial_capacity,
            const std::string& name = {}
        ) noexcept -> arc_ptr<self>;
        IR_NODISCARD static auto make(
            device_t& device,
            const descriptor_size_table& size,
            const std::string& name = {}
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkDescriptorPool;
        IR_NODISCARD auto device() const noexcept -> device_t&;
        IR_NODISCARD auto sizes() const noexcept -> std::span<const std::pair<descriptor_type_t, uint32>>;

        IR_NODISCARD auto capacity(descriptor_type_t) const noexcept -> uint32;

    private:
        VkDescriptorPool _handle;
        akl::fast_hash_map<descriptor_type_t, uint32> _sizes;

        std::reference_wrapper<device_t> _device;
    };
}
