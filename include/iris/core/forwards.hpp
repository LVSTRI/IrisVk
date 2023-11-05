#pragma once

#include <iris/core/types.hpp>

namespace ir {
    template <typename>
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
    struct buffer_create_info_t;
    struct sampler_create_info_t;
    struct texture_create_info_t;
    struct semaphore_create_info_t;

    enum class sample_count_t : uint32;
    enum class image_usage_t : uint32;
    enum class resource_format_t : uint32;
    enum class image_aspect_t : uint32;
    enum class image_layout_t : uint32;
    enum class image_flag_t : uint32;
    enum class pipeline_stage_t : uint64;
    enum class resource_access_t : uint64;
    enum class descriptor_type_t : uint32;
    enum class descriptor_binding_flag_t : uint32;
    enum class shader_stage_t : uint32;
    enum class dynamic_state_t : uint32;
    enum class cull_mode_t : uint32;
    enum class buffer_usage_t : uint32;
    enum class memory_property_t : uint32;
    enum class component_swizzle_t : uint32;
    enum class attachment_load_op_t : uint32;
    enum class attachment_store_op_t : uint32;
    enum class pipeline_bind_point_t : uint32;
    enum class command_pool_flag_t : uint32;
    enum class compare_op_t : uint32;
    enum class index_type_t : uint32;
    enum class sampler_filter_t : uint32;
    enum class sampler_mipmap_mode_t : uint32;
    enum class sampler_address_mode_t : uint32;
    enum class sampler_border_color_t : uint32;
    enum class sampler_reduction_mode_t : uint32;
    enum class primitive_topology_t : uint32;

    enum class queue_type_t;
    struct queue_family_t;
    struct queue_semaphore_stage_t;
    struct queue_submit_info_t;
    struct queue_present_info_t;
    struct queue_bind_sparse_info_t;

    struct attachment_layout_t;
    struct attachment_info_t;
    struct subpass_info_t;
    struct subpass_dependency_info_t;

    struct draw_mesh_tasks_indirect_command_t;
    struct image_memory_barrier_t;
    struct buffer_memory_barrier_t;
    struct memory_barrier_t;
    struct image_copy_t;
    struct buffer_copy_t;
    struct viewport_t;
    struct scissor_t;

    enum class clear_value_type_t;
    union clear_color_t;
    struct clear_depth_t;

    enum class pipeline_type_t;
    enum class attachment_blend_t;
    enum class depth_state_flag_t;
    enum class vertex_attribute_t;

    struct descriptor_binding_t;

    enum class buffer_flag_t;
    struct memory_properties_t;

    struct image_info_t;
    struct buffer_info_t;
    struct descriptor_content_t;
    struct descriptor_set_binding_t;

    struct offset_2d_t;
    struct offset_3d_t;
    struct extent_2d_t;
    struct extent_3d_t;
    struct image_subresource_t;

    template <typename>
    struct cache_entry_t;

    enum class keyboard_t;
    struct cursor_position_t;

    struct sampler_filter_combo_t;
    struct sampler_address_mode_combo_t;

    enum class texture_format_t;

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
    class descriptor_pool_t;
    template <typename>
    class buffer_t;
    class descriptor_set_t;
    class deletion_queue_t;
    template <typename>
    class cache_t;
    class sampler_t;
    class texture_t;

    class ngx_wrapper_t;

    class wsi_platform_t;
    class input_t;
}
