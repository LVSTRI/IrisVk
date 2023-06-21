#pragma once

#include <iris/core/types.hpp>

namespace ir {
    template <typename T>
    class intrusive_atomic_ptr_t;
    // shorthand
    template <typename T>
    using arc_ptr = intrusive_atomic_ptr_t<T>;

    struct instance_create_info_t;
    struct device_create_info_t;
    struct queue_create_info_t;
    struct swapchain_create_info_t;
    struct image_create_info_t;
    struct image_view_create_info_t;
    struct render_pass_create_info_t;
    struct command_pool_create_info_t;
    struct command_buffer_create_info_t;
    struct framebuffer_create_info_t;
    struct graphics_pipeline_create_info_t;

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
    enum class attachment_load_op_t;
    enum class attachment_store_op_t;
    enum class pipeline_bind_point_t;
    enum class command_pool_flag_t;
    enum class compare_op_t;

    enum class queue_type_t;
    struct queue_family_t;

    struct attachment_layout_t;
    struct attachment_info_t;
    struct subpass_info_t;
    struct subpass_dependency_info_t;

    struct image_memory_barrier_t;
    struct buffer_memory_barrier_t;
    struct memory_barrier_t;

    enum class clear_value_type_t;
    union clear_color_t;
    struct clear_depth_t;

    enum class pipeline_type_t;
    enum class attachment_blend_t;
    enum class depth_state_flag_t;

    struct descriptor_binding_t;

    class wsi_platform_t;
    class instance_t;
    class device_t;
    class queue_t;
    class image_t;
    class image_view_t;
    class swapchain_t;
    class render_pass_t;
    class command_pool_t;
    class command_buffer_t;
    class framebuffer_t;
    class fence_t;
    class semaphore_t;
    class clear_value_t;
    class pipeline_t;
    class master_frame_counter_t;
    class frame_counter_t;
    class descriptor_layout_t;
}
