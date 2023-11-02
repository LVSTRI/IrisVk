#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <vulkan/vulkan.h>

#include <vector>
#include <span>

namespace ir {
    enum class clear_value_type_t {
        e_none,
        e_color,
        e_depth
    };

    union clear_color_t {
        float32 f32[4] = {};
        int32 i32[4];
        uint32 u32[4];
    };

    struct clear_depth_t {
        float32 depth = {};
        uint32 stencil = {};
    };

    class clear_value_t {
    public:
        using self = clear_value_t;

        IR_CONSTEXPR clear_value_t() noexcept;
        IR_CONSTEXPR clear_value_t(const clear_color_t& color) noexcept;
        IR_CONSTEXPR clear_value_t(const clear_depth_t& depth) noexcept;
        IR_CONSTEXPR ~clear_value_t() noexcept;

        IR_CONSTEXPR clear_value_t(const self& other) noexcept;
        IR_CONSTEXPR clear_value_t(self&& other) noexcept;
        IR_CONSTEXPR auto operator =(self other) noexcept -> self&;

        IR_NODISCARD IR_CONSTEXPR auto type() const noexcept -> clear_value_type_t;
        IR_NODISCARD IR_CONSTEXPR auto color() const noexcept -> const clear_color_t&;
        IR_NODISCARD IR_CONSTEXPR auto depth() const noexcept -> const clear_depth_t&;

        friend auto swap(self& lhs, self& rhs) noexcept -> void;
    private:
        union {
            clear_color_t _color;
            clear_depth_t _depth;
        };
        clear_value_type_t _type;
    };

    IR_NODISCARD IR_CONSTEXPR auto make_clear_color(const float32(&color)[4]) noexcept -> clear_color_t;
    IR_NODISCARD IR_CONSTEXPR auto make_clear_color(const int32(&color)[4]) noexcept -> clear_color_t;
    IR_NODISCARD IR_CONSTEXPR auto make_clear_color(const uint32(&color)[4]) noexcept -> clear_color_t;
    IR_NODISCARD IR_CONSTEXPR auto make_clear_depth(float32 depth, uint32 stencil) noexcept -> clear_depth_t;

    static_assert(sizeof(clear_color_t) == sizeof(VkClearColorValue), "clear_color_t size mismatch");
    static_assert(sizeof(clear_depth_t) == sizeof(VkClearDepthStencilValue), "clear_depth_t size mismatch");

    IR_CONSTEXPR clear_value_t::clear_value_t() noexcept : _color(), _type() {}

    IR_CONSTEXPR clear_value_t::clear_value_t(const clear_color_t& color) noexcept
        : _color(color), _type(clear_value_type_t::e_color) {}

    IR_CONSTEXPR clear_value_t::clear_value_t(const clear_depth_t& depth) noexcept
        : _depth(depth), _type(clear_value_type_t::e_depth) {}

    IR_CONSTEXPR clear_value_t::~clear_value_t() noexcept = default;

    IR_CONSTEXPR clear_value_t::clear_value_t(const self& other) noexcept
        : _type(other._type) {
        switch (_type) {
            case clear_value_type_t::e_color:
                _color = other._color;
                break;

            case clear_value_type_t::e_depth:
                _depth = other._depth;
                break;

            case clear_value_type_t::e_none:
                break;
        }
    }

    IR_CONSTEXPR clear_value_t::clear_value_t(self&& other) noexcept {
        swap(*this, other);
    }

    IR_CONSTEXPR auto clear_value_t::operator =(self other) noexcept -> self& {
        swap(*this, other);
        return *this;
    }

    IR_CONSTEXPR auto clear_value_t::type() const noexcept -> clear_value_type_t {
        return _type;
    }

    IR_CONSTEXPR auto clear_value_t::color() const noexcept -> const clear_color_t& {
        IR_ASSERT(_type == clear_value_type_t::e_color, "color() on non-color clear value");
        return _color;
    }

    IR_CONSTEXPR auto clear_value_t::depth() const noexcept -> const clear_depth_t& {
        IR_ASSERT(_type == clear_value_type_t::e_depth, "depth() on non-depth clear value");
        return _depth;
    }

    inline auto swap(clear_value_t& lhs, clear_value_t& rhs) noexcept -> void {
        using std::swap;
        swap(lhs._color, rhs._color);
        swap(lhs._depth, rhs._depth);
        swap(lhs._type, rhs._type);
    }

    IR_CONSTEXPR auto make_clear_color(const float32(&color)[4]) noexcept -> clear_color_t {
        return { .f32 = {color[0], color[1], color[2], color[3] } };
    }

    IR_CONSTEXPR auto make_clear_color(const int32(&color)[4]) noexcept -> clear_color_t {
        return { .i32 = {color[0], color[1], color[2], color[3] } };
    }

    IR_CONSTEXPR auto make_clear_color(const uint32(&color)[4]) noexcept -> clear_color_t {
        return { .u32 = {color[0], color[1], color[2], color[3] } };
    }

    IR_CONSTEXPR auto make_clear_depth(float32 depth, uint32 stencil) noexcept -> clear_depth_t {
        return { .depth = depth, .stencil = stencil };
    }
}
