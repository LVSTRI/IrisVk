#pragma once

#include <ankerl/unordered_dense.h>

#include <filesystem>
#include <memory>
#include <cstdint>
#include <utility>

namespace ir {
    namespace fs = std::filesystem;

    inline namespace types {
        using int8 = std::int8_t;
        using int16 = std::int16_t;
        using int32 = std::int32_t;
        using int64 = std::int64_t;
        using uint8 = std::uint8_t;
        using uint16 = std::uint16_t;
        using uint32 = std::uint32_t;
        using uint64 = std::uint64_t;
        using float32 = float;
        using float64 = double;

        using platform_window_handle = void*;
        using gfx_api_object_handle = void*;

        using descriptor_reference = uint64;
    }

    namespace akl {
        using namespace ankerl::unordered_dense;
        namespace wyhash = detail::wyhash;

        template <
            typename K,
            typename T,
            typename H = hash<K>,
            typename E = std::equal_to<K>,
            typename A = std::allocator<std::pair<K, T>>,
            typename B = bucket_type::standard>
        using fast_hash_map = map<K, T, H, E, A, B>;

        template <
            typename K,
            typename H = hash<K>,
            typename E = std::equal_to<K>,
            typename A = std::allocator<K>,
            typename B = bucket_type::standard>
        using fast_hash_set = set<K, H, E, A, B>;
    }

    inline namespace literals {
        constexpr auto operator ""_i8(unsigned long long int value) noexcept -> int8 {
            return static_cast<int8>(value);
        }

        constexpr auto operator ""_i16(unsigned long long int value) noexcept -> int16 {
            return static_cast<int16>(value);
        }

        constexpr auto operator ""_i32(unsigned long long int value) noexcept -> int32 {
            return static_cast<int32>(value);
        }

        constexpr auto operator ""_i64(unsigned long long int value) noexcept -> int64 {
            return static_cast<int64>(value);
        }

        constexpr auto operator ""_u8(unsigned long long int value) noexcept -> uint8 {
            return static_cast<uint8>(value);
        }

        constexpr auto operator ""_u16(unsigned long long int value) noexcept -> uint16 {
            return static_cast<uint16>(value);
        }

        constexpr auto operator ""_u32(unsigned long long int value) noexcept -> uint32 {
            return static_cast<uint32>(value);
        }

        constexpr auto operator ""_u64(unsigned long long int value) noexcept -> uint64 {
            return static_cast<uint64>(value);
        }

        constexpr auto operator ""_f32(long double value) noexcept -> float32 {
            return static_cast<float32>(value);
        }

        constexpr auto operator ""_f64(long double value) noexcept -> float64 {
            return static_cast<float64>(value);
        }

        constexpr auto operator ""_KiB(unsigned long long int value) noexcept -> uint64 {
            return value * 1024;
        }

        constexpr auto operator ""_MiB(unsigned long long int value) noexcept -> uint64 {
            return value * 1024 * 1024;
        }

        constexpr auto operator ""_GiB(unsigned long long int value) noexcept -> uint64 {
            return value * 1024 * 1024 * 1024;
        }
    }

    struct offset_2d_t {
        constexpr auto operator ==(const offset_2d_t& other) const noexcept -> bool = default;

        int32 x = 0;
        int32 y = 0;
    };
    constexpr static auto ignored_offset_2d = offset_2d_t { -1_i32, -1_i32 };

    struct offset_3d_t {
        constexpr auto operator ==(const offset_3d_t& other) const noexcept -> bool = default;

        int32 x = 0;
        int32 y = 0;
        int32 z = 0;
    };
    constexpr static auto ignored_offset_3d = offset_3d_t { -1_i32, -1_i32, -1_i32 };

    struct extent_2d_t {
        constexpr auto operator ==(const extent_2d_t& other) const noexcept -> bool = default;

        uint32 width = 0;
        uint32 height = 0;
    };
    constexpr static auto ignored_extent_2d = extent_2d_t { -1_u32, -1_u32 };

    struct extent_3d_t {
        constexpr auto operator ==(const extent_3d_t& other) const noexcept -> bool = default;

        uint32 width = 0;
        uint32 height = 0;
        uint32 depth = 0;
    };
    constexpr static auto ignored_extent_3d = extent_3d_t { -1_u32, -1_u32, -1_u32 };
}
