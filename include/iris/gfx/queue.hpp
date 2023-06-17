#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <memory>

namespace ir {
    struct queue_family_t {
        uint32 family = 0;
        uint32 index = 0;

        constexpr auto operator ==(const queue_family_t& other) const noexcept -> bool {
            return
                family == other.family &&
                index == other.index;
        }
    };

    enum class queue_type_t {
        e_graphics,
        e_compute,
        e_transfer,
    };

    struct queue_create_info_t {
        queue_family_t family = {};
        queue_type_t type = {};
    };

    class queue_t : public enable_intrusive_refcount_t {
    public:
        using self = queue_t;

        queue_t(const device_t& device) noexcept;
        ~queue_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device, const queue_create_info_t& info) noexcept -> intrusive_atomic_ptr_t<self>;

        IR_NODISCARD auto handle() const noexcept -> VkQueue;
        IR_NODISCARD auto family() const noexcept -> uint32;
        IR_NODISCARD auto index() const noexcept -> uint32;
        IR_NODISCARD auto type() const noexcept -> queue_type_t;

        IR_NODISCARD auto info() const noexcept -> const queue_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;
        IR_NODISCARD auto logger() const noexcept -> const spdlog::logger&;

    private:
        VkQueue _handle = {};
        std::mutex _lock;

        queue_create_info_t _info = {};
        std::reference_wrapper<const device_t> _device;
        std::shared_ptr<spdlog::logger> _logger;
    };
}
