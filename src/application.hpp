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
#include <iris/gfx/image.hpp>
#include <iris/gfx/buffer.hpp>
#include <iris/gfx/semaphore.hpp>
#include <iris/gfx/fence.hpp>

#include <iris/wsi/wsi_platform.hpp>

#include <vector>
#include <chrono>

namespace test {
    struct view_t {
        glm::mat4 projection;
        glm::mat4 inv_projection;
        glm::mat4 view;
        glm::mat4 inv_view;
        glm::mat4 proj_view;
        glm::mat4 inv_proj_view;
        glm::vec4 eye_position;
        glm::vec4 frustum[6];
        glm::vec2 resolution;
    };

    struct transform_t {
        glm::mat4 model;
        glm::mat4 prev_model;
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

        auto _initialize() noexcept -> void;
        auto _initialize_sync() noexcept -> void;
        auto _initialize_visbuffer_pass() noexcept -> void;

        auto _visbuffer_pass() noexcept -> void;
        auto _visbuffer_resolve_pass() noexcept -> void;
        auto _visbuffer_tonemap_pass() noexcept -> void;
        auto _swapchain_copy_pass(uint32 image_index) noexcept -> void;

        uint64 _frame_index = 0;
        ir::wsi_platform_t _wsi;
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
            ir::arc_ptr<ir::image_t> depth;
            ir::arc_ptr<ir::image_t> color;
            ir::arc_ptr<ir::image_t> final;
            ir::arc_ptr<ir::render_pass_t> pass;
            ir::arc_ptr<ir::framebuffer_t> framebuffer;
            ir::arc_ptr<ir::pipeline_t> main;
            ir::arc_ptr<ir::pipeline_t> resolve;
            ir::arc_ptr<ir::pipeline_t> tonemap;
        } _visbuffer;

        struct {
            std::vector<ir::arc_ptr<ir::buffer_t<view_t>>> views;
            std::vector<ir::arc_ptr<ir::buffer_t<transform_t>>> transforms;
            std::vector<ir::arc_ptr<ir::buffer_t<uint64>>> addresses;
            ir::arc_ptr<ir::buffer_t<base_meshlet_t>> meshlets;
            ir::arc_ptr<ir::buffer_t<meshlet_instance_t>> meshlet_instances;
            ir::arc_ptr<ir::buffer_t<meshlet_vertex_format_t>> vertices;
            ir::arc_ptr<ir::buffer_t<uint32>> indices;
            ir::arc_ptr<ir::buffer_t<uint8>> primitives;
            ir::arc_ptr<ir::buffer_t<uint64>> atomics;
        } _buffer;

        camera_t _camera;

        ch::steady_clock::time_point _last_frame = {};
        float32 _delta_time = 0.0f;
    };
}