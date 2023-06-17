#include <iris/gfx/device.hpp>
#include <iris/gfx/instance.hpp>
#include <iris/gfx/swapchain.hpp>

#include <renderer/core/application.hpp>

namespace app {
    application_t::application_t() noexcept {
        _platform = ir::wsi_platform_t::make(1280, 720, "Iris");
        _instance = ir::instance_t::make({
            .features = {
                .debug_markers = true
            },
            .wsi_extensions = ir::wsi_platform_t::context_extensions()
        });
        _device = ir::device_t::make(*_instance, {
            .features = {
                .swapchain = true
            }
        });
        _swapchain = ir::swapchain_t::make(*_device, _platform, {});
    }

    application_t::~application_t() noexcept = default;

    auto application_t::run() noexcept -> void {
        while (!_platform.should_close()) {
            _update();
            _render();
        }
    }

    auto application_t::_update() noexcept -> void {
        ir::wsi_platform_t::poll_events();
    }

    auto application_t::_render() noexcept -> void {

    }
}
