#include <iris/gfx/instance.hpp>

#include <iris/wsi/wsi_platform.hpp>

int main() {
    auto wsi = ir::wsi_platform_t::make(1280, 720, "Iris");
    auto instance = ir::instance_t::make({
        .features = {
            .debug_markers = true
        },
        .wsi_extensions = ir::wsi_platform_t::context_extensions()
    });

    while (!wsi.should_close()) {
        IR_MARK_FRAME();
        ir::wsi_platform_t::poll_events();
    }
    return 0;
}
