#include <iris/wsi/wsi_platform.hpp>
#include <iris/wsi/input.hpp>

#include <array>

namespace ir {
    input_t::input_t(const wsi_platform_t& platform) noexcept
        : _platform(std::cref(platform)) {
        IR_PROFILE_SCOPED();
    }

    input_t::~input_t() noexcept = default;

    auto input_t::make(const wsi_platform_t& platform) noexcept -> arc_ptr<input_t::self> {
        auto input = arc_ptr<self>(new self(platform));
        input->reset_cursor();
        return input;
    }

    auto input_t::platform() const noexcept -> const wsi_platform_t& {
        IR_PROFILE_SCOPED();
        return _platform.get();
    }

    auto input_t::is_pressed(keyboard_t key) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _new_keys[as_underlying(key)] == GLFW_PRESS;
    }

    auto input_t::is_released(keyboard_t key) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _new_keys[as_underlying(key)] == GLFW_RELEASE;
    }

    auto input_t::is_pressed_once(keyboard_t key) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return
            _old_keys[as_underlying(key)] == GLFW_RELEASE &&
            _new_keys[as_underlying(key)] == GLFW_PRESS;
    }

    auto input_t::is_released_once(keyboard_t key) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return
            _old_keys[as_underlying(key)] == GLFW_PRESS &&
            _new_keys[as_underlying(key)] == GLFW_RELEASE;
    }

    auto input_t::is_pressed(mouse_t button) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _new_mouse[as_underlying(button)] == GLFW_PRESS;
    }

    auto input_t::is_released(mouse_t button) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _new_mouse[as_underlying(button)] == GLFW_RELEASE;
    }

    auto input_t::is_pressed_once(mouse_t button) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return
            _old_mouse[as_underlying(button)] == GLFW_RELEASE &&
            _new_mouse[as_underlying(button)] == GLFW_PRESS;
    }

    auto input_t::is_released_once(mouse_t button) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return
            _old_mouse[as_underlying(button)] == GLFW_PRESS &&
            _new_mouse[as_underlying(button)] == GLFW_RELEASE;
    }

    auto input_t::cursor_position() const noexcept -> cursor_position_t {
        IR_PROFILE_SCOPED();
        return _cursor_position;
    }

    auto input_t::cursor_delta() noexcept -> cursor_position_t {
        IR_PROFILE_SCOPED();
        const auto half_width = _platform.get().width() / 2.0;
        const auto half_height = _platform.get().height() / 2.0;
        return {
            static_cast<float32>(_cursor_position.x - half_width),
            // flipped
            static_cast<float32>(half_height - _cursor_position.y)
        };
    }

    auto input_t::reset_cursor() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto half_width = _platform.get().width() / 2.0;
        const auto half_height = _platform.get().height() / 2.0;
        _cursor_position = {
            static_cast<float32>(half_width),
            static_cast<float32>(half_height)
        };
        if (_platform.get().is_cursor_captured()) {
            glfwSetCursorPos(static_cast<GLFWwindow*>(_platform.get().window_handle()), half_width, half_height);
        }
    }

    auto input_t::capture() noexcept -> void {
        IR_PROFILE_SCOPED();
        std::memcpy(_old_keys, _new_keys, sizeof(_old_keys));
        std::memcpy(_old_mouse, _new_mouse, sizeof(_old_mouse));
        _previous_is_cursor_captured = _current_is_cursor_captured;

        constexpr static auto keys = std::to_array({
            keyboard_t::e_space,
            keyboard_t::e_apostrophe,
            keyboard_t::e_comma,
            keyboard_t::e_minus,
            keyboard_t::e_period,
            keyboard_t::e_slash,
            keyboard_t::e_0,
            keyboard_t::e_1,
            keyboard_t::e_2,
            keyboard_t::e_3,
            keyboard_t::e_4,
            keyboard_t::e_5,
            keyboard_t::e_6,
            keyboard_t::e_7,
            keyboard_t::e_8,
            keyboard_t::e_9,
            keyboard_t::e_semicolon,
            keyboard_t::e_equal,
            keyboard_t::e_a,
            keyboard_t::e_b,
            keyboard_t::e_c,
            keyboard_t::e_d,
            keyboard_t::e_e,
            keyboard_t::e_f,
            keyboard_t::e_g,
            keyboard_t::e_h,
            keyboard_t::e_i,
            keyboard_t::e_j,
            keyboard_t::e_k,
            keyboard_t::e_l,
            keyboard_t::e_m,
            keyboard_t::e_n,
            keyboard_t::e_o,
            keyboard_t::e_p,
            keyboard_t::e_q,
            keyboard_t::e_r,
            keyboard_t::e_s,
            keyboard_t::e_t,
            keyboard_t::e_u,
            keyboard_t::e_v,
            keyboard_t::e_w,
            keyboard_t::e_x,
            keyboard_t::e_y,
            keyboard_t::e_z,
            keyboard_t::e_left_bracket,
            keyboard_t::e_backslash,
            keyboard_t::e_right_bracket,
            keyboard_t::e_grave_accent,
            keyboard_t::e_world_1,
            keyboard_t::e_world_2,
            keyboard_t::e_escape,
            keyboard_t::e_enter,
            keyboard_t::e_tab,
            keyboard_t::e_backspace,
            keyboard_t::e_insert,
            keyboard_t::e_delete,
            keyboard_t::e_right,
            keyboard_t::e_left,
            keyboard_t::e_down,
            keyboard_t::e_up,
            keyboard_t::e_page_up,
            keyboard_t::e_page_down,
            keyboard_t::e_home,
            keyboard_t::e_end,
            keyboard_t::e_caps_lock,
            keyboard_t::e_scroll_lock,
            keyboard_t::e_num_lock,
            keyboard_t::e_print_screen,
            keyboard_t::e_pause,
            keyboard_t::e_f1,
            keyboard_t::e_f2,
            keyboard_t::e_f3,
            keyboard_t::e_f4,
            keyboard_t::e_f5,
            keyboard_t::e_f6,
            keyboard_t::e_f7,
            keyboard_t::e_f8,
            keyboard_t::e_f9,
            keyboard_t::e_f10,
            keyboard_t::e_f11,
            keyboard_t::e_f12,
            keyboard_t::e_f13,
            keyboard_t::e_f14,
            keyboard_t::e_f15,
            keyboard_t::e_f16,
            keyboard_t::e_f17,
            keyboard_t::e_f18,
            keyboard_t::e_f19,
            keyboard_t::e_f20,
            keyboard_t::e_f21,
            keyboard_t::e_f22,
            keyboard_t::e_f23,
            keyboard_t::e_f24,
            keyboard_t::e_f25,
            keyboard_t::e_kp_0,
            keyboard_t::e_kp_1,
            keyboard_t::e_kp_2,
            keyboard_t::e_kp_3,
            keyboard_t::e_kp_4,
            keyboard_t::e_kp_5,
            keyboard_t::e_kp_6,
            keyboard_t::e_kp_7,
            keyboard_t::e_kp_8,
            keyboard_t::e_kp_9,
            keyboard_t::e_kp_decimal,
            keyboard_t::e_kp_divide,
            keyboard_t::e_kp_multiply,
            keyboard_t::e_kp_subtract,
            keyboard_t::e_kp_add,
            keyboard_t::e_kp_enter,
            keyboard_t::e_kp_equal,
            keyboard_t::e_left_shift,
            keyboard_t::e_left_control,
            keyboard_t::e_left_alt,
            keyboard_t::e_left_super,
            keyboard_t::e_right_shift,
            keyboard_t::e_right_control,
            keyboard_t::e_right_alt,
            keyboard_t::e_right_super,
            keyboard_t::e_menu,
        });
        constexpr static auto mouse_buttons = std::to_array({
            mouse_t::e_left_button,
            mouse_t::e_right_button,
            mouse_t::e_middle_button,
        });
        auto* window = static_cast<GLFWwindow*>(_platform.get().window_handle());
        for (const auto& key : keys) {
            _new_keys[as_underlying(key)] = glfwGetKey(window, as_underlying(key));
        }
        for (const auto& button : mouse_buttons) {
            _new_mouse[as_underlying(button)] = glfwGetMouseButton(window, as_underlying(button));
        }
        if (_platform.get().is_cursor_captured()) {
            _current_is_cursor_captured = true;
            auto x = 0.0;
            auto y = 0.0;
            const auto half_width = _platform.get().width() / 2.0;
            const auto half_height = _platform.get().height() / 2.0;
            glfwGetCursorPos(window, &x, &y);
            glfwSetCursorPos(static_cast<GLFWwindow*>(_platform.get().window_handle()), half_width, half_height);
            _cursor_position.x = static_cast<float32>(x);
            _cursor_position.y = static_cast<float32>(y);
        }
    }
}
