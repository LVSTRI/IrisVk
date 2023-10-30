#pragma once

#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <iris/wsi/input.hpp>

#include <spdlog/spdlog.h>

#include <string_view>
#include <string>

namespace ir {
    // Supports only GLFW, could be extended
    class wsi_platform_t : public enable_intrusive_refcount_t<wsi_platform_t> {
    public:
        using self = wsi_platform_t;

        wsi_platform_t() noexcept;
        ~wsi_platform_t() noexcept;

        IR_NODISCARD static auto make(uint32 width, uint32 height, std::string_view title) noexcept -> arc_ptr<self>;

        static auto poll_events() noexcept -> void;
        static auto wait_events() noexcept -> void;
        IR_NODISCARD static auto context_extensions() noexcept -> std::vector<const char*>;

        IR_NODISCARD auto window_handle() const noexcept -> platform_window_handle;
        IR_NODISCARD auto width() const noexcept -> uint32;
        IR_NODISCARD auto height() const noexcept -> uint32;
        IR_NODISCARD auto title() const noexcept -> std::string_view;

        IR_NODISCARD auto is_focused() const noexcept -> bool;
        IR_NODISCARD auto is_cursor_captured() const noexcept -> bool;

        IR_NODISCARD auto input() noexcept -> input_t&;

        IR_NODISCARD auto should_close() const noexcept -> bool;

        auto capture_cursor() noexcept -> void;
        auto release_cursor() noexcept -> void;

        auto update_viewport() noexcept -> std::pair<uint32, uint32>;

        IR_NODISCARD auto make_surface(gfx_api_object_handle instance) const noexcept -> gfx_api_object_handle;

    private:
        platform_window_handle _window_handle = {};
        uint32 _width = 0;
        uint32 _height = 0;
        std::string _title;

        bool _is_cursor_captured = false;

        arc_ptr<input_t> _input;

        std::shared_ptr<spdlog::logger> _logger;
    };
}