#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <algorithm>
#include <vector>

namespace ir {
    struct deletion_queue_entry_t {
        std::function<void(device_t&)> callback;
        uint32 ttl = 0;
    };

    class deletion_queue_t {
    public:
        using self = deletion_queue_t;
        using entry = deletion_queue_entry_t;

        deletion_queue_t() noexcept;
        ~deletion_queue_t() noexcept;

        deletion_queue_t(const self& other) noexcept = default;
        deletion_queue_t(self&& other) noexcept = default;
        auto operator =(const self& other) noexcept -> self& = default;
        auto operator =(self&& other) noexcept -> self& = default;

        IR_NODISCARD static auto make(device_t& device) noexcept -> self;

        template <typename T, typename F>
        auto push(F&& callback) noexcept -> void;

        auto tick() noexcept -> void;

    private:
        std::vector<entry> _entries;

        arc_ptr<device_t> _device;
    };

    template <typename T, typename F>
    auto deletion_queue_t::push(F&& callback) noexcept -> void {
        IR_PROFILE_SCOPED();
        _entries.push_back({ std::forward<F>(callback), T::max_ttl });
    }
}
