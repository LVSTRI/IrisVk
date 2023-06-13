#pragma once

#include <iris/core/types.hpp>

#include <type_traits>
#include <functional>

namespace ir {
    template <typename T>
    concept is_container = requires (T value) {
        typename T::value_type;
        { value.size() } -> std::convertible_to<uint64>;
    };

    template <typename C>
        requires is_container<C>
    constexpr auto size_bytes(const C& container) noexcept -> uint64 {
        return container.size() * sizeof(typename C::value_type);
    }

    template <typename T>
    constexpr auto size_bytes(const T& = T()) noexcept -> uint64 {
        return sizeof(T);
    }

    template <typename T>
    constexpr auto as_const_ptr(const T& value) noexcept -> const T* {
        return static_cast<const T*>(std::addressof(const_cast<T&>(value)));
    }

    template <typename T>
    constexpr auto as_const_ptr(const T(&value)[]) noexcept -> const T* {
        return static_cast<const T*>(std::addressof(const_cast<T&>(value[0])));
    }
}
