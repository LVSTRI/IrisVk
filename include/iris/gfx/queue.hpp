#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/enums.hpp>
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

        constexpr auto operator ==(const queue_family_t& other) const noexcept -> bool = default;
        constexpr auto operator !=(const queue_family_t& other) const noexcept -> bool = default;
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

    struct queue_semaphore_stage_t {
        std::reference_wrapper<const semaphore_t> semaphore;
        pipeline_stage_t stage = pipeline_stage_t::e_none;
    };

    struct queue_submit_info_t {
        std::vector<std::reference_wrapper<const command_buffer_t>> command_buffers;
        std::vector<queue_semaphore_stage_t> wait_semaphores;
        std::vector<queue_semaphore_stage_t> signal_semaphores;
    };

    struct queue_present_info_t {
        std::reference_wrapper<const swapchain_t> swapchain;
        std::vector<std::reference_wrapper<const semaphore_t>> wait_semaphores;
        uint32 image = 0;
    };

    class queue_t : public enable_intrusive_refcount_t<queue_t> {
    public:
        using self = queue_t;

        queue_t(const device_t& device) noexcept;
        ~queue_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device, const queue_create_info_t& info) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkQueue;
        IR_NODISCARD auto family() const noexcept -> uint32;
        IR_NODISCARD auto index() const noexcept -> uint32;
        IR_NODISCARD auto type() const noexcept -> queue_type_t;

        IR_NODISCARD auto transient_pool(uint32 index) noexcept -> command_pool_t&;

        IR_NODISCARD auto info() const noexcept -> const queue_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;
        IR_NODISCARD auto logger() const noexcept -> spdlog::logger&;

        auto submit(const queue_submit_info_t& info, const fence_t* fence = nullptr) noexcept -> void;
        auto present(const queue_present_info_t& info) noexcept -> void;

    private:
        VkQueue _handle = {};
        std::mutex _lock;

        std::vector<arc_ptr<command_pool_t>> _transient_pools;

        queue_create_info_t _info = {};
        std::shared_ptr<spdlog::logger> _logger;
        std::reference_wrapper<const device_t> _device;
    };
}
