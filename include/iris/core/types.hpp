#pragma once

#include <filesystem>
#include <cstdint>

namespace ir {
    namespace fs = std::filesystem;

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
}
