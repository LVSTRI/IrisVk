#pragma once

#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <type_traits>
#include <string_view>
#include <functional>

namespace ir {
    template <typename T>
    concept is_container = requires (T value) {
        typename T::value_type;
        { value.size() } -> std::convertible_to<uint64>;
    };

    template <typename C>
        requires is_container<C>
    IR_NODISCARD constexpr auto size_bytes(const C& container) noexcept -> uint64 {
        return container.size() * sizeof(typename C::value_type);
    }

    template <typename T>
    IR_NODISCARD constexpr auto size_bytes(const T& = T()) noexcept -> uint64 {
        return sizeof(T);
    }

    template <typename T>
    IR_NODISCARD constexpr auto as_const_ptr(const T& value) noexcept -> const T* {
        return static_cast<const T*>(std::addressof(const_cast<T&>(value)));
    }

    template <typename T>
    IR_NODISCARD constexpr auto as_const_ptr(const T(&value)[]) noexcept -> const T* {
        return static_cast<const T*>(std::addressof(const_cast<T&>(value[0])));
    }

    template <typename T>
        requires std::is_enum_v<T>
    IR_NODISCARD constexpr auto internal_enum_as_string(T e) noexcept -> std::string_view;

    template <typename T>
        requires std::is_enum_v<T>
    IR_NODISCARD constexpr auto as_underlying(T e) noexcept -> std::underlying_type_t<T> {
        return static_cast<std::underlying_type_t<T>>(e);
    }

    template <typename T, typename U = std::underlying_type_t<T>, typename = std::enable_if_t<std::is_enum_v<T>>>
    IR_NODISCARD constexpr auto operator ~(const T& value) noexcept -> T {
        return static_cast<T>(~static_cast<U>(value));
    }

    template <typename T, typename U = std::underlying_type_t<T>, typename = std::enable_if_t<std::is_enum_v<T>>>
    IR_NODISCARD constexpr auto operator |(const T& left, const T& right) noexcept -> T {
        return static_cast<T>(static_cast<U>(left) | static_cast<U>(right));
    }

    template <typename T, typename U = std::underlying_type_t<T>, typename = std::enable_if_t<std::is_enum_v<T>>>
    IR_NODISCARD constexpr auto operator &(const T& left, const T& right) noexcept -> T {
        return static_cast<T>(static_cast<U>(left) & static_cast<U>(right));
    }

    template <typename T, typename U = std::underlying_type_t<T>, typename = std::enable_if_t<std::is_enum_v<T>>>
    IR_NODISCARD constexpr auto operator ^(const T& left, const T& right) noexcept -> T {
        return static_cast<T>(static_cast<U>(left) ^ static_cast<U>(right));
    }

    template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
    constexpr auto operator |=(T& left, const T& right) noexcept -> T& {
        return left = static_cast<T>(left | right);
    }

    template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
    constexpr auto operator &=(T& left, const T& right) noexcept -> T& {
        return left = static_cast<T>(left & right);
    }

    template <typename T, typename = std::enable_if_t<std::is_enum_v<T>>>
    constexpr auto operator ^=(T& left, const T& right) noexcept -> T& {
        return left = static_cast<T>(left ^ right);
    }
}
