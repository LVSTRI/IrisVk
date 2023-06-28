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
        glm::mat4 pv = {};
        glm::vec4 position = {};
        frustum_t frustum = {};
    };

    struct meshlet_glsl_t {
        uint32 vertex_offset = 0;
        uint32 index_offset = 0;
        uint32 primitive_offset = 0;
        uint32 index_count = 0;
        uint32 primitive_count = 0;
        alignas(alignof(float32)) aabb_t aabb = {};
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

        std::vector<ir::arc_ptr<ir::buffer_t<camera_data_t>>> camera_buffer;
    };

    struct hiz_pass_t {
        ir::arc_ptr<ir::image_t> hiz;
        std::vector<ir::arc_ptr<ir::image_view_t>> views;
        ir::arc_ptr<ir::sampler_t> reduction_sampler;

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
        std::vector<ir::arc_ptr<ir::semaphore_t>> _image_available;
        std::vector<ir::arc_ptr<ir::semaphore_t>> _render_done;
        std::vector<ir::arc_ptr<ir::fence_t>> _frame_fence;

        camera_t _camera;
        main_pass_t _main_pass;
        hiz_pass_t _hiz_pass;
        final_pass_t _final_pass;

        ch::steady_clock::time_point _last_time = {};
        float32 _delta_time = 0.0_f32;
        uint64 _frame_index = 0;
    };
}
