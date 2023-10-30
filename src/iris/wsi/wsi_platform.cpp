#include <iris/wsi/wsi_platform.hpp>
#include <iris/wsi/input.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace ir {
    struct __glfw_manager_t {
        __glfw_manager_t() noexcept {
            IR_PROFILE_SCOPED();
            IR_ASSERT(glfwInit(), "failed to initialize platform core");
        }

        ~__glfw_manager_t() noexcept {
            IR_PROFILE_SCOPED();
            glfwTerminate();
        }
    };

    wsi_platform_t::wsi_platform_t() noexcept = default;

    wsi_platform_t::~wsi_platform_t() noexcept = default;

    auto wsi_platform_t::make(uint32 width, uint32 height, std::string_view title) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        static auto __glfw_manager = __glfw_manager_t();
        auto logger = spdlog::stdout_color_mt("wsi");
        auto platform = arc_ptr<self>(new self());
        platform->_width = width;
        platform->_height = height;
        platform->_title = title;
        platform->_logger = logger;
        IR_LOG_INFO(logger, "initializing window (width: {}, height: {}, title: \"{}\")", width, height, title);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        platform->_window_handle = reinterpret_cast<platform_window_handle>(
            glfwCreateWindow(width, height, title.data(), nullptr, nullptr));
        glfwSetWindowUserPointer(reinterpret_cast<GLFWwindow*>(platform->_window_handle), &platform);
        IR_ASSERT(platform->_window_handle, "failed to make wsi platform");
        platform->_input = input_t::make(*platform);
        return platform;
    }

    auto wsi_platform_t::window_handle() const noexcept -> platform_window_handle {
        IR_PROFILE_SCOPED();
        return _window_handle;
    }

    auto wsi_platform_t::width() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _width;
    }

    auto wsi_platform_t::height() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _height;
    }

    auto wsi_platform_t::title() const noexcept -> std::string_view {
        IR_PROFILE_SCOPED();
        return _title;
    }

    auto wsi_platform_t::is_focused() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return glfwGetWindowAttrib(reinterpret_cast<GLFWwindow*>(_window_handle), GLFW_FOCUSED);
    }

    auto wsi_platform_t::is_cursor_captured() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _is_cursor_captured;
    }

    auto wsi_platform_t::input() noexcept -> input_t& {
        IR_PROFILE_SCOPED();
        return *_input;
    }

    auto wsi_platform_t::poll_events() noexcept -> void {
        IR_PROFILE_SCOPED();
        glfwPollEvents();
    }

    auto wsi_platform_t::wait_events() noexcept -> void {
        IR_PROFILE_SCOPED();
        glfwWaitEvents();
    }

    auto wsi_platform_t::should_close() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return glfwWindowShouldClose(reinterpret_cast<GLFWwindow*>(_window_handle));
    }

    auto wsi_platform_t::capture_cursor() noexcept -> void {
        IR_PROFILE_SCOPED();
        glfwSetInputMode(reinterpret_cast<GLFWwindow*>(_window_handle), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        _input->reset_cursor();
        _is_cursor_captured = true;
    }

    auto wsi_platform_t::release_cursor() noexcept -> void {
        IR_PROFILE_SCOPED();
        glfwSetInputMode(reinterpret_cast<GLFWwindow*>(_window_handle), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        _input->reset_cursor();
        _is_cursor_captured = false;
    }

    auto wsi_platform_t::update_viewport() noexcept -> std::pair<uint32, uint32> {
        IR_PROFILE_SCOPED();
        auto width = 0_i32;
        auto height = 0_i32;
        glfwGetFramebufferSize(reinterpret_cast<GLFWwindow*>(_window_handle), &width, &height);
        if (width != _width || height != _height) {
            _input->reset_cursor();
        }
        _width = static_cast<uint32>(width);
        _height = static_cast<uint32>(height);
        return { _width, _height };
    }

    auto wsi_platform_t::context_extensions() noexcept -> std::vector<const char*> {
        IR_PROFILE_SCOPED();
        auto count = 0_u32;
        const auto** extensions = glfwGetRequiredInstanceExtensions(&count);
        return { extensions, extensions + count };
    }

    auto wsi_platform_t::make_surface(gfx_api_object_handle instance) const noexcept -> gfx_api_object_handle {
        IR_PROFILE_SCOPED();
        auto surface = VkSurfaceKHR();
        IR_VULKAN_CHECK(_logger, glfwCreateWindowSurface(
            static_cast<VkInstance>(instance),
            reinterpret_cast<GLFWwindow*>(_window_handle),
            nullptr,
            &surface));
        return static_cast<gfx_api_object_handle>(surface);
    }
}
