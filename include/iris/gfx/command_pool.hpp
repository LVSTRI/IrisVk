#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <optional>
#include <vector>
#include <memory>
#include <span>

namespace ir {
    struct command_pool_create_info_t {
        queue_type_t queue = {};
        command_pool_flag_t flags = {};
    };

    class command_pool_t : public enable_intrusive_refcount_t<command_pool_t> {
    public:
        using self = command_pool_t;

        command_pool_t(const device_t& device) noexcept;
        ~command_pool_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device, const command_pool_create_info_t& info) noexcept -> arc_ptr<self>;

        IR_NODISCARD static auto make(
            const device_t& device,
            uint32 count,
            const command_pool_create_info_t& info
        ) noexcept -> std::vector<arc_ptr<self>>;

        IR_NODISCARD auto handle() const noexcept -> VkCommandPool;
        IR_NODISCARD auto info() const noexcept -> const command_pool_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;

        auto reset() const noexcept -> void;
    private:
        VkCommandPool _handle = {};

        command_pool_create_info_t _info = {};
        std::reference_wrapper<const device_t> _device;
    };
}
