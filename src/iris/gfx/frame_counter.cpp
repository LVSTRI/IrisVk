#include <iris/gfx/frame_counter.hpp>

namespace ir {
    master_frame_counter_t::master_frame_counter_t() noexcept = default;

    master_frame_counter_t::~master_frame_counter_t() noexcept = default;

    auto master_frame_counter_t::make(uint32 current) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto master = arc_ptr<self>(new self());
        master->_current_frame = current;
        return master;
    }

    auto master_frame_counter_t::current() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _current_frame.load(std::memory_order_acquire);
    }

    auto master_frame_counter_t::fork() const noexcept -> frame_counter_t {
        IR_PROFILE_SCOPED();
        return frame_counter_t(*this);
    }

    auto master_frame_counter_t::tick() noexcept -> void {
        IR_PROFILE_SCOPED();
        _current_frame.fetch_add(1, std::memory_order_release);
    }

    auto master_frame_counter_t::reset() noexcept -> void {
        IR_PROFILE_SCOPED();
        _current_frame.store(0, std::memory_order_release);
    }

    frame_counter_t::frame_counter_t(const master_frame_counter_t& master) noexcept
        : _master(master.as_intrusive_ptr()) {
        IR_PROFILE_SCOPED();
        _current_frame = master.current();
    }

    frame_counter_t::~frame_counter_t() noexcept = default;

    frame_counter_t::frame_counter_t(const self& other) noexcept = default;

    frame_counter_t::frame_counter_t(self&& other) noexcept = default;

    auto frame_counter_t::operator =(const self& other) noexcept -> self& = default;

    auto frame_counter_t::operator =(self&& other) noexcept -> self& = default;

    auto frame_counter_t::master() const noexcept -> const master_frame_counter_t& {
        IR_PROFILE_SCOPED();
        return *_master;
    }

    auto frame_counter_t::current_absolute() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return master().current();
    }

    auto frame_counter_t::current_relative() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return current_absolute() - _current_frame;
    }

    auto frame_counter_t::reset() noexcept -> void {
        IR_PROFILE_SCOPED();
        _current_frame = current_absolute();
    }
}
