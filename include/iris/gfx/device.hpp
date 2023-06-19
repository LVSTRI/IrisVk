#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <memory>

namespace ir {
    struct device_features_t {
        bool swapchain = false;
    };

    struct device_create_info_t {
        device_features_t features = {};
    };

    class device_t : public enable_intrusive_refcount_t<device_t> {
    public:
        using self = device_t;

        device_t() noexcept;
        ~device_t() noexcept;

        IR_NODISCARD static auto make(
            const instance_t& instance,
            const device_create_info_t& info = {}) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkDevice;
        IR_NODISCARD auto gpu() const noexcept -> VkPhysicalDevice;
        IR_NODISCARD auto allocator() const noexcept -> VmaAllocator;

        IR_NODISCARD auto graphics_queue() const noexcept -> const queue_t&;
        IR_NODISCARD auto compute_queue() const noexcept -> const queue_t&;
        IR_NODISCARD auto transfer_queue() const noexcept -> const queue_t&;

        IR_NODISCARD auto info() const noexcept -> const device_create_info_t&;
        IR_NODISCARD auto instance() const noexcept -> const instance_t&;
        IR_NODISCARD auto logger() const noexcept -> spdlog::logger&;

        IR_NODISCARD auto fetch_queue(const queue_family_t& family) const noexcept -> VkQueue;

        auto wait_idle() const noexcept -> void;

    private:
        VkDevice _handle = {};
        VkPhysicalDevice _gpu = {};
        VmaAllocator _allocator = {};

        VkPhysicalDeviceProperties2 _properties = {};
        VkPhysicalDeviceMemoryProperties2 _memory_properties = {};
        VkPhysicalDeviceFeatures2 _features = {};
        VkPhysicalDeviceVulkan11Features _features_11 = {};
        VkPhysicalDeviceVulkan12Features _features_12 = {};
        VkPhysicalDeviceVulkan13Features _features_13 = {};

        arc_ptr<queue_t> _graphics;
        arc_ptr<queue_t> _compute;
        arc_ptr<queue_t> _transfer;

        device_create_info_t _info = {};
        arc_ptr<const instance_t> _instance;
        std::shared_ptr<spdlog::logger> _logger;
    };
}
