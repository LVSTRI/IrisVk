#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <optional>
#include <vector>
#include <memory>
#include <span>

namespace ir {
    struct framebuffer_create_info_t {
        std::vector<arc_ptr<const image_t>> attachments = {};
        // TODO
    };

    class framebuffer_t : public enable_intrusive_refcount_t<framebuffer_t> {
    public:
        using self = framebuffer_t;

        framebuffer_t() noexcept;
        ~framebuffer_t() noexcept;

        IR_NODISCARD static auto make(const render_pass_t& render_pass, const framebuffer_create_info_t& info) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkFramebuffer;
        IR_NODISCARD auto attachments() const noexcept -> std::span<const arc_ptr<const image_t>>;
        IR_NODISCARD auto attachment(uint32 index) const noexcept -> const image_t&;
        IR_NODISCARD auto info() const noexcept -> const framebuffer_create_info_t&;
        IR_NODISCARD auto render_pass() const noexcept -> const render_pass_t&;

        IR_NODISCARD auto width() const noexcept -> uint32;
        IR_NODISCARD auto height() const noexcept -> uint32;
        IR_NODISCARD auto layers() const noexcept -> uint32;

    private:
        VkFramebuffer _handle = {};

        framebuffer_create_info_t _info = {};
        arc_ptr<const render_pass_t> _render_pass;
    };
}
