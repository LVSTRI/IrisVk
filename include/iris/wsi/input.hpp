#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <GLFW/glfw3.h>

namespace ir {
    enum class keyboard_t {
        e_space = 32,
        e_apostrophe = 39,
        e_comma = 44,
        e_minus = 45,
        e_period = 46,
        e_slash = 47,
        e_0 = 48,
        e_1 = 49,
        e_2 = 50,
        e_3 = 51,
        e_4 = 52,
        e_5 = 53,
        e_6 = 54,
        e_7 = 55,
        e_8 = 56,
        e_9 = 57,
        e_semicolon = 59,
        e_equal = 61,
        e_a = 65,
        e_b = 66,
        e_c = 67,
        e_d = 68,
        e_e = 69,
        e_f = 70,
        e_g = 71,
        e_h = 72,
        e_i = 73,
        e_j = 74,
        e_k = 75,
        e_l = 76,
        e_m = 77,
        e_n = 78,
        e_o = 79,
        e_p = 80,
        e_q = 81,
        e_r = 82,
        e_s = 83,
        e_t = 84,
        e_u = 85,
        e_v = 86,
        e_w = 87,
        e_x = 88,
        e_y = 89,
        e_z = 90,
        e_left_bracket = 91,
        e_backslash = 92,
        e_right_bracket = 93,
        e_grave_accent = 96,
        e_world_1 = 161,
        e_world_2 = 162,
        e_escape = 256,
        e_enter = 257,
        e_tab = 258,
        e_backspace = 259,
        e_insert = 260,
        e_delete = 261,
        e_right = 262,
        e_left = 263,
        e_down = 264,
        e_up = 265,
        e_page_up = 266,
        e_page_down = 267,
        e_home = 268,
        e_end = 269,
        e_caps_lock = 280,
        e_scroll_lock = 281,
        e_num_lock = 282,
        e_print_screen = 283,
        e_pause = 284,
        e_f1 = 290,
        e_f2 = 291,
        e_f3 = 292,
        e_f4 = 293,
        e_f5 = 294,
        e_f6 = 295,
        e_f7 = 296,
        e_f8 = 297,
        e_f9 = 298,
        e_f10 = 299,
        e_f11 = 300,
        e_f12 = 301,
        e_f13 = 302,
        e_f14 = 303,
        e_f15 = 304,
        e_f16 = 305,
        e_f17 = 306,
        e_f18 = 307,
        e_f19 = 308,
        e_f20 = 309,
        e_f21 = 310,
        e_f22 = 311,
        e_f23 = 312,
        e_f24 = 313,
        e_f25 = 314,
        e_kp_0 = 320,
        e_kp_1 = 321,
        e_kp_2 = 322,
        e_kp_3 = 323,
        e_kp_4 = 324,
        e_kp_5 = 325,
        e_kp_6 = 326,
        e_kp_7 = 327,
        e_kp_8 = 328,
        e_kp_9 = 329,
        e_kp_decimal = 330,
        e_kp_divide = 331,
        e_kp_multiply = 332,
        e_kp_subtract = 333,
        e_kp_add = 334,
        e_kp_enter = 335,
        e_kp_equal = 336,
        e_left_shift = 340,
        e_left_control = 341,
        e_left_alt = 342,
        e_left_super = 343,
        e_right_shift = 344,
        e_right_control = 345,
        e_right_alt = 346,
        e_right_super = 347,
        e_menu = 348,
        e_count
    };

    enum class mouse_t {
        e_left_button,
        e_right_button,
        e_middle_button,
        e_count
    };

    struct cursor_position_t {
        float32 x = 0.0f;
        float32 y = 0.0f;
    };

    class input_t : public enable_intrusive_refcount_t<input_t> {
    public:
        using self = input_t;

        input_t(const wsi_platform_t& platform) noexcept;
        ~input_t() noexcept;

        IR_NODISCARD static auto make(const wsi_platform_t& platform) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto platform() const noexcept -> const wsi_platform_t&;

        IR_NODISCARD auto is_pressed(keyboard_t key) const noexcept -> bool;
        IR_NODISCARD auto is_released(keyboard_t key) const noexcept -> bool;
        IR_NODISCARD auto is_pressed_once(keyboard_t key) const noexcept -> bool;
        IR_NODISCARD auto is_released_once(keyboard_t key) const noexcept -> bool;

        IR_NODISCARD auto is_pressed(mouse_t button) const noexcept -> bool;
        IR_NODISCARD auto is_released(mouse_t button) const noexcept -> bool;
        IR_NODISCARD auto is_pressed_once(mouse_t button) const noexcept -> bool;
        IR_NODISCARD auto is_released_once(mouse_t button) const noexcept -> bool;

        IR_NODISCARD auto cursor_position() const noexcept -> cursor_position_t;
        IR_NODISCARD auto cursor_delta() noexcept -> cursor_position_t;

        auto reset_cursor() noexcept -> void;

        auto capture() noexcept -> void;

    private:
        int32 _old_keys[as_underlying(keyboard_t::e_count)] = {};
        int32 _new_keys[as_underlying(keyboard_t::e_count)] = {};

        int32 _old_mouse[as_underlying(mouse_t::e_count)] = {};
        int32 _new_mouse[as_underlying(mouse_t::e_count)] = {};

        bool _previous_is_cursor_captured = false;
        bool _current_is_cursor_captured = false;

        cursor_position_t _cursor_position = {};

        std::reference_wrapper<const wsi_platform_t> _platform;
    };
}
