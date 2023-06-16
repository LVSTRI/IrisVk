#pragma once

#include <iris/core/types.hpp>
#include <iris/core/macros.hpp>

#include <atomic>

namespace ir {
    class enable_intrusive_refcount_t {
    public:
        using self = enable_intrusive_refcount_t;

        enable_intrusive_refcount_t() noexcept;
        ~enable_intrusive_refcount_t() noexcept;

        enable_intrusive_refcount_t(const self&) noexcept = delete;
        enable_intrusive_refcount_t(self&& other) noexcept = delete;
        auto operator =(self other) noexcept -> self& = delete;

        auto count() const noexcept -> uint64;
        auto grab() const noexcept -> uint64;
        auto drop() const noexcept -> uint64;

    private:
        mutable std::atomic<uint64> _count = 0;
    };

    template <typename T>
    class intrusive_atomic_ptr_t {
    public:
        using self = intrusive_atomic_ptr_t;
        intrusive_atomic_ptr_t() noexcept;
        intrusive_atomic_ptr_t(std::nullptr_t) noexcept;
        intrusive_atomic_ptr_t(T* ptr) noexcept;
        ~intrusive_atomic_ptr_t() noexcept;

        intrusive_atomic_ptr_t(const self& other) noexcept;
        intrusive_atomic_ptr_t(self&& other) noexcept;
        auto operator =(self other) noexcept -> self&;

        auto reset() noexcept -> void;

        IR_NODISCARD auto get() const noexcept -> T*;
        IR_NODISCARD auto as_ref() const noexcept -> T&;
        IR_NODISCARD auto as_const_ref() const noexcept -> const T&;
        IR_NODISCARD auto release() noexcept -> T*;

        IR_NODISCARD auto operator *() noexcept -> T&;
        IR_NODISCARD auto operator *() const noexcept -> const T&;
        IR_NODISCARD auto operator ->() const noexcept -> T*;

        IR_NODISCARD auto operator ==(const self& other) const noexcept -> bool;
        IR_NODISCARD auto operator !=(const self& other) const noexcept -> bool;

        IR_NODISCARD auto operator ==(std::nullptr_t) const noexcept -> bool;
        IR_NODISCARD auto operator !=(std::nullptr_t) const noexcept -> bool;

        IR_NODISCARD auto operator !() const noexcept -> bool;

        IR_NODISCARD operator bool() const noexcept;

        template <typename U>
        friend auto swap(intrusive_atomic_ptr_t<U>& lhs, intrusive_atomic_ptr_t<U>& rhs) noexcept -> void;

    private:
        T* _ptr = nullptr;
    };

    template <typename T>
    intrusive_atomic_ptr_t(T*) -> intrusive_atomic_ptr_t<T>;

    template <typename T>
    intrusive_atomic_ptr_t<T>::intrusive_atomic_ptr_t() noexcept = default;

    template <typename T>
    intrusive_atomic_ptr_t<T>::intrusive_atomic_ptr_t(std::nullptr_t) noexcept : self() {}

    template <typename T>
    intrusive_atomic_ptr_t<T>::intrusive_atomic_ptr_t(T* ptr) noexcept : _ptr(ptr) {
        IR_PROFILE_SCOPED();
        if (ptr) {
            ptr->grab();
        }
    }

    template <typename T>
    intrusive_atomic_ptr_t<T>::~intrusive_atomic_ptr_t() noexcept {
        IR_PROFILE_SCOPED();
        reset();
    }

    template <typename T>
    intrusive_atomic_ptr_t<T>::intrusive_atomic_ptr_t(const self& other) noexcept
        : self(other._ptr) {
        IR_PROFILE_SCOPED();
    }

    template <typename T>
    intrusive_atomic_ptr_t<T>::intrusive_atomic_ptr_t(self&& other) noexcept {
        IR_PROFILE_SCOPED();
        swap(*this, other);
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::operator =(self other) noexcept -> self& {
        IR_PROFILE_SCOPED();
        swap(*this, other);
        return *this;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::reset() noexcept -> void {
        IR_PROFILE_SCOPED();
        if (_ptr) {
            if (_ptr->drop() == 0) {
               delete _ptr;
            }
        }
        _ptr = nullptr;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::get() const noexcept -> T* {
        IR_PROFILE_SCOPED();
        return _ptr;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::as_ref() const noexcept -> T& {
        IR_PROFILE_SCOPED();
        return *_ptr;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::as_const_ref() const noexcept -> const T& {
        IR_PROFILE_SCOPED();
        return *_ptr;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::release() noexcept -> T* {
        IR_PROFILE_SCOPED();
        if (!_ptr) {
            return nullptr;
        }
        IR_ASSERT(_ptr->drop() == 0, "release(): invalid reference count");
        return std::exchange(_ptr, nullptr);
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::operator *() noexcept -> T& {
        IR_PROFILE_SCOPED();
        return as_ref();
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::operator *() const noexcept -> const T& {
        IR_PROFILE_SCOPED();
        return as_const_ref();
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::operator ->() const noexcept -> T* {
        IR_PROFILE_SCOPED();
        return _ptr;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::operator ==(const self& other) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _ptr == other._ptr;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::operator !=(const self& other) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _ptr != other._ptr;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::operator ==(std::nullptr_t) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _ptr == nullptr;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::operator !=(std::nullptr_t) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _ptr != nullptr;
    }

    template <typename T>
    auto intrusive_atomic_ptr_t<T>::operator !() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return !_ptr;
    }

    template <typename T>
    intrusive_atomic_ptr_t<T>::operator bool() const noexcept {
        IR_PROFILE_SCOPED();
        return _ptr;
    }

    template <typename T>
    auto swap(intrusive_atomic_ptr_t<T>& lhs, intrusive_atomic_ptr_t<T>& rhs) noexcept -> void {
        IR_PROFILE_SCOPED();
        using std::swap;
        swap(lhs._ptr, rhs._ptr);
    }
}
