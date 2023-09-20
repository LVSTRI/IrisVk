#pragma once

#include <renderer/core/camera.hpp>
#include <renderer/core/utilities.hpp>
#include <renderer/core/model.hpp>

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>

#include <iris/wsi/wsi_platform.hpp>

#include <glm/glm.hpp>

namespace app {
    struct camera_data_t {
        glm::mat4 projection = {};
        glm::mat4 view = {};
        glm::mat4 old_pv = {};
        glm::mat4 pv = {};
        glm::vec4 position = {};
        frustum_t frustum = {};
    };

    struct state_t {
        uint32 view_mode = 1;
        bool enable_hw = true;
        bool enable_sw = true;
    };

    struct meshlet_material_glsl_t {
        uint32 base_color_texture = -1;
        uint32 normal_texture = -1;
    };

    struct meshlet_glsl_t {
        uint32 vertex_offset = 0;
        uint32 index_offset = 0;
        uint32 primitive_offset = 0;
        uint32 index_count = 0;
        uint32 primitive_count = 0;
        alignas(alignof(float32)) aabb_t aabb = {};
        alignas(alignof(uint32)) meshlet_material_glsl_t material = {};
    };

    struct main_pass_t {
        ir::arc_ptr<ir::render_pass_t> description;
        ir::arc_ptr<ir::framebuffer_t> framebuffer;
        ir::arc_ptr<ir::image_t> visbuffer;

        ir::arc_ptr<ir::pipeline_t> pipeline;

        ir::arc_ptr<ir::buffer_t<meshlet_glsl_t>> meshlets;
        ir::arc_ptr<ir::buffer_t<meshlet_vertex_format_t>> vertices;
        ir::arc_ptr<ir::buffer_t<meshlet_instance_t>> meshlet_instances;
        ir::arc_ptr<ir::buffer_t<uint32>> indices;
        ir::arc_ptr<ir::buffer_t<uint8>> primitives;
        ir::arc_ptr<ir::buffer_t<glm::mat4>> transforms;
        std::vector<ir::arc_ptr<ir::texture_t>> textures;

        ir::arc_ptr<ir::buffer_t<uint32>> atomics;
        std::vector<ir::arc_ptr<ir::buffer_t<camera_data_t>>> camera_buffer;
    };

    struct cluster_classify_pass_t {
        ir::arc_ptr<ir::pipeline_t> pipeline;

        ir::arc_ptr<ir::buffer_t<uint32>> sw_rast;
        ir::arc_ptr<ir::buffer_t<uint32>> hw_rast;
        ir::arc_ptr<ir::buffer_t<glm::uvec3>> sw_command;
        ir::arc_ptr<ir::buffer_t<glm::uvec3>> hw_command;
    };

    struct computer_raster_pass_t {
        ir::arc_ptr<ir::pipeline_t> pipeline;
    };

    struct shadow_pass_t {
        ir::arc_ptr<ir::image_t> map;
        ir::arc_ptr<ir::pipeline_t> analyze;
        ir::arc_ptr<ir::sampler_t> sampler;

        ir::arc_ptr<ir::image_t> hzb;
        ir::arc_ptr<ir::pipeline_t> reduce;
        std::vector<ir::arc_ptr<ir::image_view_t>> hzb_views;
        ir::arc_ptr<ir::buffer_t<uint8>> device_requests;

        ir::arc_ptr<ir::buffer_t<uint32>> sw_rast;
        ir::arc_ptr<ir::buffer_t<uint32>> hw_rast;
        ir::arc_ptr<ir::buffer_t<glm::uvec3>> sw_command;
        ir::arc_ptr<ir::buffer_t<glm::uvec3>> hw_command;

        std::vector<ir::arc_ptr<ir::buffer_t<glm::mat4>>> camera;
        std::vector<ir::arc_ptr<ir::buffer_t<uint8>>> host_requests;
    };

    struct hiz_pass_t {
        ir::arc_ptr<ir::image_t> depth;
        std::vector<ir::arc_ptr<ir::image_view_t>> views;
        ir::arc_ptr<ir::sampler_t> sampler;

        ir::arc_ptr<ir::pipeline_t> copy;
        ir::arc_ptr<ir::pipeline_t> reduce;
    };

    struct final_pass_t {
        ir::arc_ptr<ir::render_pass_t> description;
        ir::arc_ptr<ir::framebuffer_t> framebuffer;
        ir::arc_ptr<ir::image_t> color;
        std::vector<ir::clear_value_t> clear_values;

        ir::arc_ptr<ir::pipeline_t> pipeline;

   };

    class application_t {
    public:
        application_t() noexcept;
        ~application_t() noexcept;
        application_t(const application_t&) = delete;
        application_t(application_t&&) noexcept = delete;
        auto operator =(application_t) -> application_t& = delete;

        auto run() noexcept -> void;

        auto wsi_platform() noexcept -> ir::wsi_platform_t&;
        auto device() noexcept -> ir::device_t&;
        auto swapchain() noexcept -> ir::swapchain_t&;
        auto command_pool(uint32 frame_index = -1) noexcept -> ir::command_pool_t&;
        auto command_buffer(uint32 frame_index = -1) noexcept -> ir::command_buffer_t&;
        auto image_available(uint32 frame_index = -1) noexcept -> ir::semaphore_t&;
        auto render_done(uint32 frame_index = -1) noexcept -> ir::semaphore_t&;
        auto frame_fence(uint32 frame_index = -1) noexcept -> ir::fence_t&;

        auto camera_buffer(uint32 frame_index = -1) noexcept -> ir::buffer_t<camera_data_t>&;

    private:
        auto _render() noexcept -> void;
        auto _update() noexcept -> void;

        ir::wsi_platform_t _platform;
        ir::arc_ptr<ir::instance_t> _instance;
        ir::arc_ptr<ir::device_t> _device;
        ir::arc_ptr<ir::swapchain_t> _swapchain;
        std::vector<ir::arc_ptr<ir::command_pool_t>> _command_pools;
        std::vector<ir::arc_ptr<ir::command_buffer_t>> _command_buffers;

        ir::arc_ptr<ir::semaphore_t> _sparse_done;
        std::vector<ir::arc_ptr<ir::semaphore_t>> _image_available;
        std::vector<ir::arc_ptr<ir::semaphore_t>> _render_done;
        std::vector<ir::arc_ptr<ir::fence_t>> _frame_fence;

        camera_t _camera;
        main_pass_t _main_pass;
        cluster_classify_pass_t _cluster_pass;
        shadow_pass_t _shadow_pass;
        computer_raster_pass_t _compute_rast_pass;
        hiz_pass_t _hiz_pass;
        final_pass_t _final_pass;

        state_t _state = {};

        ch::steady_clock::time_point _last_time = {};
        float32 _delta_time = 0.0_f32;
        uint64 _frame_index = 0;
    };
}
