#include "iris/core/utilities.hpp"

#include "iris/gfx/device.hpp"
#include "iris/gfx/queue.hpp"

#include "spdlog/sinks/stdout_color_sinks.h"

namespace ir {
    template <>
    constexpr auto as_string(queue_type_t type) noexcept -> std::string_view {
        switch (type) {
            case queue_type_t::graphics: return "graphics";
            case queue_type_t::compute: return "compute";
            case queue_type_t::transfer: return "transfer";
        }
        return "";
    }

    queue_t::queue_t(const device_t& device) noexcept
        : _device(std::cref(device)) {}

    queue_t::~queue_t() noexcept {
        IR_PROFILE_SCOPED();
        IR_LOG_INFO(_logger, "queue destroyed");
    }

    auto queue_t::make(const device_t& device, const queue_create_info_t& info) noexcept -> intrusive_atomic_ptr_t<self> {
        IR_PROFILE_SCOPED();
        auto queue = intrusive_atomic_ptr_t(new self(device));
        auto logger = spdlog::stdout_color_mt(as_string(info.type).data());
        IR_LOG_INFO(logger, "queue initialized (family: {}, index: {})", info.family.family, info.family.index);
        queue->_handle = device.fetch_queue(info.family);
        queue->_info = info;
        queue->_logger = std::move(logger);
        return queue;
    }

    auto queue_t::handle() const noexcept -> VkQueue {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto queue_t::family() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _info.family.family;
    }

    auto queue_t::index() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _info.family.index;
    }

    auto queue_t::type() const noexcept -> queue_type_t {
        IR_PROFILE_SCOPED();
        return _info.type;
    }

    auto queue_t::info() const noexcept -> const queue_create_info_t& {
        return _info;
    }

    auto queue_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return _device.get();
    }

    auto queue_t::logger() const noexcept -> const spdlog::logger& {
        IR_PROFILE_SCOPED();
        return *_logger;
    }
}
