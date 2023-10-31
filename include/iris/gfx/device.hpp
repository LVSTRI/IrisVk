#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/hash.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <iris/gfx/frame_counter.hpp>
#include <iris/gfx/deletion_queue.hpp>
#include <iris/gfx/sampler.hpp>
#include <iris/gfx/descriptor_layout.hpp>
#include <iris/gfx/descriptor_set.hpp>
#include <iris/gfx/cache.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <memory>

namespace ir {
    struct device_features_t {
        bool swapchain = false;
        bool mesh_shader = false;
        bool image_atomics_64 = false;
        bool fragment_shading_rate = false;
        bool ray_tracing = false;
#if defined(IRIS_NVIDIA_DLSS)
        bool dlss = false;
#endif
    };

    struct device_create_info_t {
        device_features_t features = {};
    };

    enum class device_feature_t {
        e_buffer_device_address
    };

    class device_t : public enable_intrusive_refcount_t<device_t> {
    public:
        using self = device_t;

        device_t() noexcept;
        ~device_t() noexcept;

        IR_NODISCARD static auto make(
            const instance_t& instance,
            const device_create_info_t& info = {}
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkDevice;
        IR_NODISCARD auto gpu() const noexcept -> VkPhysicalDevice;
        IR_NODISCARD auto allocator() const noexcept -> VmaAllocator;

        IR_NODISCARD auto properties() const noexcept -> const VkPhysicalDeviceProperties&;
        IR_NODISCARD auto memory_properties() const noexcept -> const VkPhysicalDeviceMemoryProperties&;

#if defined(IRIS_NVIDIA_DLSS)
        IR_NODISCARD auto ngx() noexcept -> ngx_wrapper_t&;
#endif

        IR_NODISCARD auto graphics_queue() noexcept -> queue_t&;
        IR_NODISCARD auto graphics_queue() const noexcept -> const queue_t&;
        IR_NODISCARD auto compute_queue() noexcept -> queue_t&;
        IR_NODISCARD auto compute_queue() const noexcept -> const queue_t&;
        IR_NODISCARD auto transfer_queue() noexcept -> queue_t&;
        IR_NODISCARD auto transfer_queue() const noexcept -> const queue_t&;

        IR_NODISCARD auto descriptor_pool() const noexcept -> const descriptor_pool_t&;

        IR_NODISCARD auto frame_counter() noexcept -> master_frame_counter_t&;
        IR_NODISCARD auto frame_counter() const noexcept -> const master_frame_counter_t&;
        IR_NODISCARD auto deletion_queue() noexcept -> deletion_queue_t&;

        IR_NODISCARD auto info() const noexcept -> const device_create_info_t&;
        IR_NODISCARD auto instance() const noexcept -> const instance_t&;
        IR_NODISCARD auto logger() const noexcept -> spdlog::logger&;

        IR_NODISCARD auto fetch_queue(const queue_family_t& family) const noexcept -> VkQueue;
        IR_NODISCARD auto memory_type_index(uint32 mask, memory_property_t flags) const noexcept -> uint32;

        auto wait_idle() const noexcept -> void;

        auto resize_descriptor_pool(const akl::fast_hash_map<descriptor_type_t, uint32>& size) noexcept -> void;

        template <typename T>
        IR_NODISCARD auto cache() noexcept -> cache_t<T>&;

        IR_NODISCARD auto is_supported(device_feature_t feature) const noexcept -> bool;

        auto tick() noexcept -> void;

    private:
        VkDevice _handle = {};
        VkPhysicalDevice _gpu = {};
        VmaAllocator _allocator = {};

        VkPhysicalDeviceProperties2 _properties = {};
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR _properties_rt = {};
        VkPhysicalDeviceMemoryProperties2 _memory_properties = {};
        VkPhysicalDeviceFeatures2 _features = {};
        VkPhysicalDeviceVulkan11Features _features_11 = {};
        VkPhysicalDeviceVulkan12Features _features_12 = {};
        VkPhysicalDeviceVulkan13Features _features_13 = {};

#if defined(IRIS_NVIDIA_DLSS)
        std::unique_ptr<ngx_wrapper_t> _ngx;
#endif

        arc_ptr<queue_t> _graphics;
        arc_ptr<queue_t> _compute;
        arc_ptr<queue_t> _transfer;

        arc_ptr<descriptor_pool_t> _descriptor_pool;

        arc_ptr<master_frame_counter_t> _frame_counter;
        deletion_queue_t _deletion_queue;

        cache_t<descriptor_layout_t> _descriptor_layouts;
        cache_t<descriptor_set_t> _descriptor_sets;
        cache_t<sampler_t> _samplers;

        device_create_info_t _info = {};
        arc_ptr<const instance_t> _instance;
        std::shared_ptr<spdlog::logger> _logger;
    };
}
