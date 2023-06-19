#include <iris/gfx/device.hpp>
#include <iris/gfx/render_pass.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/framebuffer.hpp>

namespace ir {
    framebuffer_t::framebuffer_t() noexcept = default;

    framebuffer_t::~framebuffer_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroyFramebuffer(_render_pass.as_const_ref().device().handle(), _handle, nullptr);
        IR_LOG_INFO(_render_pass.as_const_ref().device().logger(), "framebuffer {} destroyed", fmt::ptr(_handle));
    }

    auto framebuffer_t::make(const render_pass_t& render_pass, const framebuffer_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto framebuffer = ir::arc_ptr<self>(new self());

        auto attachments = std::vector<VkImageView>();
        attachments.reserve(info.attachments.size());
        for (const auto& attachment : info.attachments) {
            attachments.push_back(attachment.as_const_ref().view().handle());
        }

        const auto width = info.attachments[0].as_const_ref().width();
        const auto height = info.attachments[0].as_const_ref().height();
        const auto layers = info.attachments[0].as_const_ref().layers();

        auto framebuffer_info = VkFramebufferCreateInfo();
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.pNext = nullptr;
        framebuffer_info.flags = {};
        framebuffer_info.renderPass = render_pass.handle();
        framebuffer_info.attachmentCount = attachments.size();
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = width;
        framebuffer_info.height = height;
        framebuffer_info.layers = layers;
        IR_VULKAN_CHECK(
            render_pass.device().logger(),
            vkCreateFramebuffer(render_pass.device().handle(), &framebuffer_info, nullptr, &framebuffer->_handle));
        IR_LOG_INFO(render_pass.device().logger(), "framebuffer {} created", fmt::ptr(framebuffer->_handle));

        framebuffer->_attachments = info.attachments;
        framebuffer->_info = info;
        framebuffer->_render_pass = render_pass.as_intrusive_ptr();
        return framebuffer;
    }

    auto framebuffer_t::handle() const noexcept -> VkFramebuffer {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto framebuffer_t::attachments() const noexcept -> std::span<const arc_ptr<const image_t>> {
        IR_PROFILE_SCOPED();
        return _attachments;
    }

    auto framebuffer_t::info() const noexcept -> const framebuffer_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto framebuffer_t::render_pass() const noexcept -> const render_pass_t& {
        IR_PROFILE_SCOPED();
        return *_render_pass;
    }
}
