#include <iris/gfx/deletion_queue.hpp>
#include <iris/gfx/device.hpp>

namespace ir {
    deletion_queue_t::deletion_queue_t() noexcept {
        IR_PROFILE_SCOPED();
        _entries.reserve(128);
    }

    deletion_queue_t::~deletion_queue_t() noexcept = default;

    auto deletion_queue_t::make(device_t& device) noexcept -> self {
        IR_PROFILE_SCOPED();
        auto queue = self();
        queue._device = device.as_intrusive_ptr();
        return queue;
    }

    auto deletion_queue_t::tick() noexcept -> void {
        IR_PROFILE_SCOPED();
        _entries.erase(std::remove_if(_entries.begin(), _entries.end(), [](entry& entry) {
            if (entry.ttl-- == 0) {
                IR_LOG_WARN(spdlog::get("device"), "deletion_queue_t: TTL expired for object with callback {}", fmt::ptr(&entry.callback));
                return true;
            }
            return false;
        }), _entries.end());
    }
}
