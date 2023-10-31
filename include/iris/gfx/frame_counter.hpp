#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <optional>
#include <vector>
#include <memory>
#include <span>

namespace ir {
    class master_frame_counter_t : public enable_intrusive_refcount_t<master_frame_counter_t> {
    public:
        using self = master_frame_counter_t;

        master_frame_counter_t() noexcept;
        ~master_frame_counter_t() noexcept;

        IR_NODISCARD static auto make(uint64 current = 0) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto current() const noexcept -> uint64;
        IR_NODISCARD auto fork() const noexcept -> frame_counter_t;

        auto tick() noexcept -> void;
        auto reset() noexcept -> void;

    private:
        std::atomic<uint64> _current_frame = 0;
    };

    class frame_counter_t {
    public:
        using self = frame_counter_t;

        frame_counter_t() noexcept;
        frame_counter_t(const master_frame_counter_t& master) noexcept;
        ~frame_counter_t() noexcept;

        frame_counter_t(const self& other) noexcept;
        frame_counter_t(self&& other) noexcept;
        auto operator =(const self& other) noexcept -> self&;
        auto operator =(self&& other) noexcept -> self&;

        IR_NODISCARD auto master() const noexcept -> const master_frame_counter_t&;

        IR_NODISCARD auto current_absolute() const noexcept -> uint64;
        IR_NODISCARD auto current_relative() const noexcept -> uint64;
        auto reset() noexcept -> void;

    private:
        uint64 _current_frame = 0;

        arc_ptr<const master_frame_counter_t> _master;
    };
}
