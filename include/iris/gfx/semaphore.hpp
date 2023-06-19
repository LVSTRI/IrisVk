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
    class semaphore_t : public enable_intrusive_refcount_t<semaphore_t> {
    public:
        using self = semaphore_t;

        semaphore_t() noexcept;
        ~semaphore_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device) noexcept -> arc_ptr<self>;
        IR_NODISCARD static auto make(const device_t& device, uint32 count) noexcept -> std::vector<arc_ptr<self>>;

        IR_NODISCARD auto handle() const noexcept -> VkSemaphore;
        IR_NODISCARD auto device() const noexcept -> const device_t&;

    private:
        VkSemaphore _handle = {};

        arc_ptr<const device_t> _device;
    };
}