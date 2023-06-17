#pragma once

#include <iris/core/types.hpp>

namespace ir {
    template <typename T>
    class intrusive_atomic_ptr_t;

    struct instance_create_info_t;
    struct device_create_info_t;
    struct queue_create_info_t;
    struct swapchain_create_info_t;
    struct image_create_info_t;
    struct image_view_create_info_t;

    enum class sample_count_t;
    enum class image_usage_t;
    enum class resource_format_t;
    enum class image_aspect_t;
    enum class image_layout_t;
    enum class pipeline_stage_t : uint64;
    enum class resource_access_t : uint64;
    enum class descriptor_type_t;
    enum class shader_stage_t;
    enum class dynamic_state_t;
    enum class cull_mode_t;
    enum class buffer_usage_t;
    enum class memory_property_t;
    enum class component_swizzle_t;

    enum class queue_type_t;
    struct queue_family_t;

    class wsi_platform_t;
    class instance_t;
    class device_t;
    class queue_t;
    class image_t;
    class image_view_t;
    class swapchain_t;
}
