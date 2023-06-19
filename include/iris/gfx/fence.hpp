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
    class fence_t : public enable_intrusive_refcount_t<fence_t> {
    public:
        using self = fence_t;

        fence_t() noexcept;
        ~fence_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device, bool signaled = true) noexcept -> arc_ptr<self>;
        IR_NODISCARD static auto make(const device_t& device, uint32 count, bool signaled = true) noexcept -> std::vector<arc_ptr<self>>;

        IR_NODISCARD auto handle() const noexcept -> VkFence;
        IR_NODISCARD auto device() const noexcept -> const device_t&;

        IR_NODISCARD auto is_ready() const noexcept -> bool;

        auto wait(uint64 timeout = -1_u64) const noexcept -> void;
        auto reset() const noexcept -> void;

    private:
        VkFence _handle = {};

        arc_ptr<const device_t> _device;
    };
}