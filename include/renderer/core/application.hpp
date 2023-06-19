#pragma once

#include <renderer/core/utilities.hpp>

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>

#include <iris/wsi/wsi_platform.hpp>

namespace app {
    struct main_pass_t {
        ir::arc_ptr<ir::render_pass_t> description;
        ir::arc_ptr<ir::image_t> color;
        ir::arc_ptr<ir::image_t> depth;
        ir::arc_ptr<ir::framebuffer_t> framebuffer;
        std::vector<ir::clear_value_t> clear_values;
    };

    class application_t {
    public:
        application_t() noexcept;
        ~application_t() noexcept;
        application_t(const application_t&) = delete;
        application_t(application_t&&) noexcept = delete;
        auto operator =(application_t) -> application_t& = delete;

        auto run() noexcept -> void;

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

        main_pass_t _main_pass;

        ch::steady_clock::time_point _last_time = {};
        float32 _delta_time = 0.0_f32;
        uint64 _frame_count = 0;
        uint64 _frame_index = 0;
    };
}
