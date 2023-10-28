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
    struct semaphore_create_info_t {
        uint64 counter = 0;
        bool timeline = false;
    };

    class semaphore_t : public enable_intrusive_refcount_t<semaphore_t> {
    public:
        using self = semaphore_t;

        semaphore_t() noexcept;
        ~semaphore_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device, const semaphore_create_info_t& info) noexcept -> arc_ptr<self>;
        IR_NODISCARD static auto make(const device_t& device, uint32 count, const semaphore_create_info_t& info) noexcept -> std::vector<arc_ptr<self>>;

        IR_NODISCARD auto handle() const noexcept -> VkSemaphore;
        IR_NODISCARD auto counter() const noexcept -> uint64;
        IR_NODISCARD auto is_timeline() const noexcept -> bool;
        IR_NODISCARD auto device() const noexcept -> const device_t&;

        auto increment(uint64 x = 1) noexcept -> uint64;

    private:
        VkSemaphore _handle = {};
        uint64 _counter = 0;
        bool _is_timeline = false;

        arc_ptr<const device_t> _device;
    };
}