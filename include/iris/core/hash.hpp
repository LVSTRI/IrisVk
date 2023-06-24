#pragma once

#include <iris/core/enums.hpp>
#include <iris/core/forwards.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <ankerl/unordered_dense.h>

#include <variant>
#include <vector>
#include <span>

template <typename... Ts>
struct ir::akl::hash<std::variant<Ts...>> {
    using is_avalanching = void;
    using is_transparent = void;
    IR_NODISCARD constexpr auto operator ()(const std::variant<Ts...>& value) const noexcept -> std::size_t {
        return std::visit([]<typename T>(const T& value) {
            return ir::akl::hash<T>()(value);
        }, value);
    }
};

template <typename T>
struct ir::akl::hash<std::vector<T>> {
    using is_avalanching = void;
    using is_transparent = void;
    IR_NODISCARD constexpr auto operator ()(const std::vector<T>& value) const noexcept -> std::size_t {
        auto seed = std::size_t();
        for (const auto& element : value) {
            seed = ir::akl::wyhash::mix(seed, ir::akl::hash<T>()(element));
        }
        return seed;
    }
};

template <typename T>
struct ir::akl::hash<std::span<T>> {
    using is_avalanching = void;
    using is_transparent = void;
    IR_NODISCARD constexpr auto operator ()(const std::span<T>& value) const noexcept -> std::size_t {
        auto seed = std::size_t();
        for (const auto& element : value) {
            seed = ir::akl::wyhash::mix(seed, ir::akl::hash<T>()(element));
        }
        return seed;
    }
};

template <typename T>
struct ir::akl::hash<ir::arc_ptr<T>> {
    using is_avalanching = void;
    using is_transparent = void;
    IR_NODISCARD constexpr auto operator ()(const ir::arc_ptr<T>& value) const noexcept -> std::size_t {
        return ir::akl::hash<T*>()(value.get());
    }
};

template <typename T>
struct std::equal_to<std::vector<T>> {
    using is_transparent = void;
    IR_NODISCARD constexpr auto operator ()(const std::vector<T>& lhs, const std::vector<T>& rhs) const noexcept -> bool {
        return lhs == rhs;
    }
};

template <typename T>
struct std::equal_to<std::span<T>> {
    using is_transparent = void;
    IR_NODISCARD constexpr auto operator ()(const std::span<T>& lhs, const std::span<T>& rhs) const noexcept -> bool {
        return lhs == rhs;
    }
};

template <typename T>
struct std::equal_to<ir::arc_ptr<T>> {
    using is_transparent = void;
    IR_NODISCARD constexpr auto operator ()(const ir::arc_ptr<T>& lhs, const ir::arc_ptr<T>& rhs) const noexcept -> bool {
        return lhs == rhs;
    }
};
