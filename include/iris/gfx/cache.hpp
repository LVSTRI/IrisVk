#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/hash.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <utility>
#include <vector>

namespace ir {
    template <typename T>
    struct cache_entry_t {
        T value;
        uint32 ttl = 0;
    };

    template <typename T>
    class cache_t {
    public:
        using self = cache_t;
        using key_type = typename T::cache_key_type;
        using value_type = typename T::cache_value_type;
        using cache_entry_type = cache_entry_t<value_type>;

        cache_t() noexcept = default;
        ~cache_t() noexcept = default;

        cache_t(const self& other) noexcept = default;
        cache_t(self&& other) noexcept = default;
        auto operator =(const self& other) noexcept -> self& = default;
        auto operator =(self&& other) noexcept -> self& = default;

        IR_NODISCARD auto acquire(const key_type& key) noexcept -> value_type& {
            IR_PROFILE_SCOPED();
            auto& entry = _map.at(key);
            entry.ttl = _max_ttl;
            return entry.value;
        }

        IR_NODISCARD auto contains(const key_type& key) const noexcept -> bool {
            IR_PROFILE_SCOPED();
            return _map.contains(key);
        }

        auto insert(const key_type& key, const value_type& value) noexcept -> const value_type& {
            IR_PROFILE_SCOPED();
            const auto [ptr, _0] = _map.try_emplace(key, cache_entry_type { value, _max_ttl });
            const auto& [_1, entry] = *ptr;
            return entry;
        }

        auto insert(const key_type& key, value_type&& value) noexcept -> const value_type& {
            IR_PROFILE_SCOPED();
            const auto [ptr, _0] = _map.try_emplace(key, cache_entry_type { std::move(value), _max_ttl });
            const auto& [_1, entry] = *ptr;
            return entry.value;
        }

        auto remove(const key_type& key) noexcept -> void {
            IR_PROFILE_SCOPED();
            _map.erase(key);
        }

        auto tick() noexcept -> void {
            IR_PROFILE_SCOPED();
            if constexpr (!_is_persistent) {
                std::erase_if(_map, [](auto& entry) {
                    if (entry.second.ttl-- == 0) {
                        IR_LOG_INFO(spdlog::get("cache"), "cache_t: TTL expired for object {}", fmt::ptr(&entry.second.value));
                        return true;
                    }
                    return false;
                });
            }
        }

        auto clear() noexcept -> void {
            IR_PROFILE_SCOPED();
            _map.clear();
        }

    private:
        constexpr static auto _max_ttl = T::max_ttl;
        constexpr static auto _is_persistent = T::is_persistent;

        akl::fast_hash_map<key_type, cache_entry_type> _map;
    };
}
