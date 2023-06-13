#include <iris/core/intrusive_atomic_ptr.hpp>

namespace ir {
    enable_intrusive_refcount_t::enable_intrusive_refcount_t() noexcept = default;

    enable_intrusive_refcount_t::~enable_intrusive_refcount_t() noexcept = default;

    auto enable_intrusive_refcount_t::count() const noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        return _count.load();
    }

    auto enable_intrusive_refcount_t::grab() const noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        return ++_count;
    }

    auto enable_intrusive_refcount_t::drop() const noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        return --_count;
    }
}
