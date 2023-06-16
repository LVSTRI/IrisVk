#include "iris/wsi/wsi_platform.hpp"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "spdlog/sinks/stdout_color_sinks.h"

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

    wsi_platform_t::wsi_platform_t(self&& other) noexcept : self() {
        IR_PROFILE_SCOPED();
        swap(*this, other);
    }

    auto wsi_platform_t::operator =(self other) noexcept -> self& {
        IR_PROFILE_SCOPED();
        swap(*this, other);
        return *this;
    }

    auto wsi_platform_t::make(uint32 width, uint32 height, std::string_view title) noexcept -> self {
        IR_PROFILE_SCOPED();
        static auto __glfw_manager = __glfw_manager_t();
        auto logger = spdlog::stdout_color_mt("wsi");
        auto platform = self();
        platform._width = width;
        platform._height = height;
        platform._title = title;
        platform._logger = logger;
        IR_LOG_INFO(logger, "initializing window (width: {}, height: {}, title: \"{}\")", width, height, title);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        platform._window_handle = reinterpret_cast<platform_window_handle>(
            glfwCreateWindow(width, height, title.data(), nullptr, nullptr));
        IR_ASSERT(platform._window_handle, "failed to make wsi platform");

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

    auto wsi_platform_t::poll_events() noexcept -> void {
        IR_PROFILE_SCOPED();
        glfwPollEvents();
    }

    auto wsi_platform_t::should_close() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return glfwWindowShouldClose(reinterpret_cast<GLFWwindow*>(_window_handle));
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
            reinterpret_cast<VkInstance>(instance),
            reinterpret_cast<GLFWwindow*>(_window_handle),
            nullptr,
            &surface));
        return reinterpret_cast<gfx_api_object_handle>(surface);
    }

    auto swap(wsi_platform_t& lhs, wsi_platform_t& rhs) noexcept -> void {
        IR_PROFILE_SCOPED();
        using std::swap;
        swap(lhs._window_handle, rhs._window_handle);
        swap(lhs._width, rhs._width);
        swap(lhs._height, rhs._height);
        swap(lhs._title, rhs._title);
        swap(lhs._logger, rhs._logger);
    }
}