#pragma once

#include <utilities.hpp>
#include <camera.hpp>
#include <model.hpp>

#include <iris/gfx/instance.hpp>
#include <iris/gfx/device.hpp>
#include <iris/gfx/swapchain.hpp>
#include <iris/gfx/command_pool.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/render_pass.hpp>
#include <iris/gfx/pipeline.hpp>
#include <iris/gfx/framebuffer.hpp>
#include <iris/gfx/descriptor_pool.hpp>
#include <iris/gfx/queue.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/buffer.hpp>
#include <iris/gfx/semaphore.hpp>
#include <iris/gfx/fence.hpp>
#include <iris/gfx/texture.hpp>

#if defined(IRIS_NVIDIA_DLSS)
    #include <iris/nvidia/ngx_wrapper.hpp>
#endif

#include <iris/wsi/wsi_platform.hpp>

#include <imgui.h>
#include <implot.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <vector>
#include <chrono>
#include <queue>

#define IRIS_MAIN_VIEW_INDEX 0
#define IRIS_SHADOW_VIEW_START 1

#define IRIS_TEXTURE_VIEWER_TYPE_2D_SFLOAT 0
#define IRIS_TEXTURE_VIEWER_TYPE_2D_SINT 1
#define IRIS_TEXTURE_VIEWER_TYPE_2D_UINT 2
#define IRIS_TEXTURE_VIEWER_TYPE_2D_ARRAY_SFLOAT 3
#define IRIS_TEXTURE_VIEWER_TYPE_2D_ARRAY_SINT 4
#define IRIS_TEXTURE_VIEWER_TYPE_2D_ARRAY_UINT 5

#define IRIS_MAX_DIRECTIONAL_LIGHTS 4
#define IRIS_MAX_SPARSE_BINDING_UPDATES 16384
#define IRIS_VSM_VIRTUAL_BASE_SIZE 8192
#define IRIS_VSM_VIRTUAL_BASE_RESOLUTION (IRIS_VSM_VIRTUAL_BASE_SIZE * IRIS_VSM_VIRTUAL_BASE_SIZE)
#define IRIS_VSM_VIRTUAL_PAGE_SIZE 128
#define IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE (IRIS_VSM_VIRTUAL_BASE_SIZE / IRIS_VSM_VIRTUAL_PAGE_SIZE)
#define IRIS_VSM_VIRTUAL_PAGE_RESOLUTION (IRIS_VSM_VIRTUAL_PAGE_SIZE * IRIS_VSM_VIRTUAL_PAGE_SIZE)
#define IRIS_VSM_VIRTUAL_PAGE_COUNT (IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE * IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE)
#define IRIS_VSM_PHYSICAL_BASE_SIZE 8192
#define IRIS_VSM_PHYSICAL_PAGE_SIZE 128
#define IRIS_VSM_PHYSICAL_PAGE_ROW_SIZE (IRIS_VSM_PHYSICAL_BASE_SIZE / IRIS_VSM_PHYSICAL_PAGE_SIZE)
#define IRIS_VSM_PHYSICAL_PAGE_RESOLUTION (IRIS_VSM_PHYSICAL_PAGE_SIZE * IRIS_VSM_PHYSICAL_PAGE_SIZE)
#define IRIS_VSM_PHYSICAL_PAGE_COUNT (IRIS_VSM_PHYSICAL_PAGE_ROW_SIZE * IRIS_VSM_PHYSICAL_PAGE_ROW_SIZE)
#define IRIS_VSM_MAX_CLIPMAPS 32
#define IRIS_VSM_CLIPMAP_COUNT 16

namespace test {
    struct view_t {
        glm::mat4 projection = {};
        glm::mat4 prev_projection = {};
        glm::mat4 inv_projection = {};
        glm::mat4 inv_prev_projection = {};
        glm::mat4 jittered_projection = {};
        glm::mat4 view = {};
        glm::mat4 prev_view = {};
        glm::mat4 inv_view = {};
        glm::mat4 inv_prev_view = {};
        glm::mat4 stable_view = {};
        glm::mat4 inv_stable_view = {};
        glm::mat4 proj_view = {};
        glm::mat4 prev_proj_view = {};
        glm::mat4 inv_proj_view = {};
        glm::mat4 inv_prev_proj_view = {};
        glm::mat4 stable_proj_view = {};
        glm::mat4 inv_stable_proj_view = {};
        glm::vec4 eye_position = {};
        glm::vec4 frustum[6] = {};
        glm::vec2 resolution = {};
    };

    struct transform_t {
        glm::mat4 model = {};
        glm::mat4 prev_model = model;
    };

    struct directional_light_t {
        glm::vec3 direction = {};
        float32 intensity = 0.0f;
    };

    struct vsm_global_data_t {
        float32 first_width = 12.0f;
        float32 lod_bias = -1.5f;
        float32 resolution_lod_bias = 0.0f;
        uint32 clipmap_count = IRIS_VSM_CLIPMAP_COUNT;
        glm::ivec2 clipmap_page_offsets[IRIS_VSM_MAX_CLIPMAPS] = {};
    };

    template <typename T>
    struct readback_t {
        ir::arc_ptr<ir::buffer_t<T>> device;
        std::vector<ir::arc_ptr<ir::buffer_t<T>>> host;
    };

    struct hzb_t {
        ir::arc_ptr<ir::sampler_t> sampler;
        ir::arc_ptr<ir::image_t> image;
        std::vector<ir::arc_ptr<ir::image_view_t>> views;
    };

    class application_t {
    public:
        application_t() noexcept;
        ~application_t() noexcept;

        auto run() noexcept -> void;

    private:
        auto _load_models() noexcept -> void;

        auto _current_frame_data() noexcept;
        auto _current_frame_buffers() noexcept;

        auto _render() noexcept -> void;
        auto _update() noexcept -> void;
        auto _update_frame_data() noexcept -> void;
        auto _resize() noexcept -> void;
        auto _resize_main_viewport() noexcept -> bool;

        auto _initialize() noexcept -> void;
        auto _initialize_dlss() noexcept -> void;
        auto _initialize_imgui() noexcept -> void;
        auto _initialize_sync() noexcept -> void;
        auto _initialize_visbuffer_pass() noexcept -> void;
        auto _initialize_vsm_pass() noexcept -> void;

        auto _clear_buffer_pass() noexcept -> void;
        auto _visbuffer_pass() noexcept -> void;
        auto _visbuffer_resolve_pass() noexcept -> void;
        auto _visbuffer_dlss_pass() noexcept -> void;
        auto _visbuffer_tonemap_pass() noexcept -> void;
        auto _vsm_mark_visible_pages_pass() noexcept -> void;
        auto _vsm_reduce_page_table_pass() noexcept -> void;
        auto _vsm_request_pages_pass() noexcept -> void;
        auto _vsm_free_pages_pass() noexcept -> void;
        auto _vsm_allocate_pages_pass() noexcept -> void;
        auto _vsm_rasterize_hardware_pass() noexcept -> void;
        auto _debug_emit_barriers() noexcept -> void;
        auto _gui_main_pass() noexcept -> void;
        auto _swapchain_copy_pass(uint32 image_index) noexcept -> void;

        uint64 _frame_index = 0;
        ir::arc_ptr<ir::wsi_platform_t> _wsi;
        ir::arc_ptr<ir::instance_t> _instance;
        ir::arc_ptr<ir::device_t> _device;
        ir::arc_ptr<ir::swapchain_t> _swapchain;
        std::vector<ir::arc_ptr<ir::command_pool_t>> _command_pools;
        std::vector<ir::arc_ptr<ir::command_buffer_t>> _command_buffers;
        std::vector<ir::arc_ptr<ir::semaphore_t>> _image_available;
        std::vector<ir::arc_ptr<ir::semaphore_t>> _render_done;
        std::vector<ir::arc_ptr<ir::fence_t>> _frame_fence;

        struct {
            bool is_initialized = false;
            ir::arc_ptr<ir::image_t> ids;
            ir::arc_ptr<ir::image_t> velocity;
            ir::arc_ptr<ir::image_t> depth;
            ir::arc_ptr<ir::image_t> color;
            ir::arc_ptr<ir::image_t> color_resolve;
            ir::arc_ptr<ir::image_t> final;
            ir::arc_ptr<ir::render_pass_t> pass;
            ir::arc_ptr<ir::framebuffer_t> framebuffer;
            ir::arc_ptr<ir::pipeline_t> main;
            ir::arc_ptr<ir::pipeline_t> resolve;
            ir::arc_ptr<ir::pipeline_t> tonemap;

            glm::mat4 prev_projection = {};
            glm::mat4 prev_view = {};
        } _visbuffer;

        struct {
            bool is_initialized = false;
            ir::arc_ptr<ir::render_pass_t> main_pass;
            ir::arc_ptr<ir::framebuffer_t> main_framebuffer;
            ir::arc_ptr<ir::image_t> phys_memory;
            ir::arc_ptr<ir::image_view_t> phys_memory_view;

            ir::arc_ptr<ir::pipeline_t> mark_visible_pages;
            ir::arc_ptr<ir::pipeline_t> make_allocation_requests;
            ir::arc_ptr<ir::pipeline_t> reduce_page_table;
            ir::arc_ptr<ir::pipeline_t> cull_active_pages;
            ir::arc_ptr<ir::pipeline_t> allocate_pages;
            ir::arc_ptr<ir::pipeline_t> free_pages;
            ir::arc_ptr<ir::pipeline_t> rasterize_hardware;

            ir::arc_ptr<ir::buffer_t<uint8>> visible_pages_buffer;
            ir::arc_ptr<ir::buffer_t<uint32>> allocation_request_buffer;
            ir::arc_ptr<ir::buffer_t<uint32>> phys_page_table_buffer;
            ir::arc_ptr<ir::buffer_t<uint32>> virt_page_table_buffer;
            ir::arc_ptr<ir::buffer_t<uint32>> meshlet_survivors_buffer;
            ir::arc_ptr<ir::buffer_t<ir::draw_mesh_tasks_indirect_command_t>> draw_indirect_buffer;

            hzb_t hzb;

            std::vector<ir::arc_ptr<ir::buffer_t<vsm_global_data_t>>> globals_buffer;
        } _vsm;

        struct {
            bool is_initialized = false;

            ir::arc_ptr<ir::render_pass_t> main_pass;
            ir::arc_ptr<ir::framebuffer_t> main_framebuffer;

            ir::arc_ptr<ir::pipeline_t> texture_viewer;
            ir::arc_ptr<ir::image_t> texture_viewer_image;

            ir::arc_ptr<ir::image_t> main_image;
            ir::arc_ptr<ir::descriptor_layout_t> main_layout;
            ir::arc_ptr<ir::sampler_t> main_sampler;

            glm::uvec2 main_viewport_resolution = { 1280, 720 };
        } _gui;

        struct {
            std::vector<ir::arc_ptr<ir::buffer_t<view_t>>> views;
            std::vector<ir::arc_ptr<ir::buffer_t<transform_t>>> transforms;
            std::vector<ir::arc_ptr<ir::buffer_t<material_t>>> materials;
            std::vector<ir::arc_ptr<ir::buffer_t<directional_light_t>>> directional_lights;
            ir::arc_ptr<ir::buffer_t<base_meshlet_t>> meshlets;
            ir::arc_ptr<ir::buffer_t<meshlet_instance_t>> meshlet_instances;
            ir::arc_ptr<ir::buffer_t<meshlet_vertex_format_t>> vertices;
            ir::arc_ptr<ir::buffer_t<uint32>> indices;
            ir::arc_ptr<ir::buffer_t<uint8>> primitives;
            ir::arc_ptr<ir::buffer_t<uint64>> atomics;
        } _buffer;

        struct scene_t {
            std::vector<ir::arc_ptr<ir::texture_t>> textures;
            std::vector<material_t> materials;

            ir::arc_ptr<ir::sampler_t> main_sampler;
        } _scene;

        struct state_t {
            std::vector<directional_light_t> directional_lights = {};

            struct {
                ir::dlss_quality_preset_t quality = ir::dlss_quality_preset_t::e_native;
                float32 lod_bias = 0.0f;
                uint32 jitter_count = 8;
                bool reset = false;

                bool is_initialized = false;
                glm::uvec2 render_resolution = { 1280, 720 };
            } dlss;

            struct {
                float32 elevation = 260.0f;
                float32 azimuth = 30.0f;
                float32 light_intensity = 4.0f;
                vsm_global_data_t global_data = {};

                uint32 hzb_vis_layer = 0;
                uint32 hzb_vis_level = 0;
            } vsm;

            struct {
                const char* current_texture_viewer = nullptr;
                const ir::image_view_t* current_texture = nullptr;
                int32 texture_viewer_layer = 0;
                int32 texture_viewer_level = 0;
            } gui;

            struct {
                std::deque<float32> frame_times = {};
            } performance;
        } _state;

        camera_t _camera;

        ch::steady_clock::time_point _last_frame = {};
        float32 _delta_time = 0.0f;
    };
}