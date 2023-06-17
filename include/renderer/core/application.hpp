#pragma once

#include <renderer/core/utilities.hpp>

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>

#include <iris/wsi/wsi_platform.hpp>

namespace app {
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
        ir::intrusive_atomic_ptr_t<ir::instance_t> _instance;
        ir::intrusive_atomic_ptr_t<ir::device_t> _device;
        ir::intrusive_atomic_ptr_t<ir::swapchain_t> _swapchain;
    };
}
