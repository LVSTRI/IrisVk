#include <iris/gfx/device.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/render_pass.hpp>

namespace ir {
    render_pass_t::render_pass_t() noexcept = default;

    render_pass_t::~render_pass_t() noexcept {
        IR_PROFILE_SCOPED();
        IR_LOG_INFO(device().logger(), "render pass destroyed");
        vkDestroyRenderPass(device().handle(), _handle, nullptr);
    }

    auto render_pass_t::make(const device_t& device, const render_pass_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto render_pass = arc_ptr<self>(new self());
        auto attachments = std::vector<VkAttachmentDescription2>();
        attachments.reserve(info.attachments.size());
        for (const auto& attachment : info.attachments) {
            auto attachment_description = VkAttachmentDescription2();
            attachment_description.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            attachment_description.pNext = nullptr;
            attachment_description.flags = 0;
            attachment_description.format = as_enum_counterpart(attachment.format);
            attachment_description.samples = as_enum_counterpart(attachment.samples);
            attachment_description.loadOp = as_enum_counterpart(attachment.load_op);
            attachment_description.storeOp = as_enum_counterpart(attachment.store_op);
            attachment_description.stencilLoadOp = as_enum_counterpart(attachment.stencil_load_op);
            attachment_description.stencilStoreOp = as_enum_counterpart(attachment.stencil_store_op);
            attachment_description.initialLayout = as_enum_counterpart(attachment.layout.initial);
            attachment_description.finalLayout = as_enum_counterpart(attachment.layout.final);
            attachments.emplace_back(attachment_description);
        }
        auto subpasses = std::vector<VkSubpassDescription2>();
        subpasses.reserve(info.subpasses.size());

        struct subpass_attachments_t {
            std::vector<VkAttachmentReference2> color_attachments = {};
            std::optional<VkAttachmentReference2> depth_stencil_attachment = std::nullopt;
            std::vector<VkAttachmentReference2> input_attachments = {};
        };
        auto attachment_references = std::vector<subpass_attachments_t>();
        attachment_references.reserve(info.subpasses.size());
        for (const auto& subpass : info.subpasses) {
            auto& references = attachment_references.emplace_back();
            for (const auto& color_attachment : subpass.color_attachments) {
                auto reference = VkAttachmentReference2();
                reference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
                reference.pNext = nullptr;
                reference.attachment = color_attachment;
                reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                reference.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                references.color_attachments.emplace_back(reference);
            }
            if (auto depth = subpass.depth_stencil_attachment) {
                auto reference = VkAttachmentReference2();
                reference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
                reference.pNext = nullptr;
                reference.attachment = *depth;
                reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                reference.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
                references.depth_stencil_attachment = reference;
            }
            for (const auto& input_attachment : subpass.input_attachments) {
                auto reference = VkAttachmentReference2();
                reference.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
                reference.pNext = nullptr;
                reference.attachment = input_attachment;
                reference.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                reference.aspectMask =
                    VK_IMAGE_ASPECT_COLOR_BIT |
                    VK_IMAGE_ASPECT_DEPTH_BIT |
                    VK_IMAGE_ASPECT_STENCIL_BIT;
                references.input_attachments.emplace_back(reference);
            }

            auto subpass_description = VkSubpassDescription2();
            subpass_description.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
            subpass_description.pNext = nullptr;
            subpass_description.flags = {};
            subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass_description.viewMask = 0;
            subpass_description.inputAttachmentCount = references.input_attachments.size();
            subpass_description.pInputAttachments = references.input_attachments.data();
            subpass_description.colorAttachmentCount = references.color_attachments.size();
            subpass_description.pColorAttachments = references.color_attachments.data();
            subpass_description.pResolveAttachments = nullptr;
            if (references.depth_stencil_attachment) {
                subpass_description.pDepthStencilAttachment = &references.depth_stencil_attachment.value();
            }
            subpass_description.preserveAttachmentCount = subpass.preserve_attachments.size();
            subpass_description.pPreserveAttachments = subpass.preserve_attachments.data();
            subpasses.emplace_back(subpass_description);
        }

        auto dependencies = std::vector<VkSubpassDependency2>();
        dependencies.reserve(info.dependencies.size());
        auto memory_barriers = std::vector<VkMemoryBarrier2>();
        memory_barriers.reserve(info.dependencies.size());
        for (const auto& dependency : info.dependencies) {
            auto memory_barrier = VkMemoryBarrier2();
            memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            memory_barrier.srcStageMask = as_enum_counterpart(dependency.source_stage);
            memory_barrier.srcAccessMask = as_enum_counterpart(dependency.source_access);
            memory_barrier.dstStageMask = as_enum_counterpart(dependency.dest_stage);
            memory_barrier.dstAccessMask = as_enum_counterpart(dependency.dest_access);

            auto dependency_description = VkSubpassDependency2();
            dependency_description.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
            dependency_description.pNext = &memory_barriers.emplace_back(memory_barrier);
            dependency_description.srcSubpass = dependency.source;
            dependency_description.dstSubpass = dependency.dest;
            dependency_description.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            dependencies.emplace_back(dependency_description);
        }

        auto render_pass_info = VkRenderPassCreateInfo2();
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
        render_pass_info.pNext = nullptr;
        render_pass_info.flags = {};
        render_pass_info.attachmentCount = attachments.size();
        render_pass_info.pAttachments = attachments.data();
        render_pass_info.subpassCount = subpasses.size();
        render_pass_info.pSubpasses = subpasses.data();
        render_pass_info.dependencyCount = dependencies.size();
        render_pass_info.pDependencies = dependencies.data();
        render_pass_info.correlatedViewMaskCount = 0;
        render_pass_info.pCorrelatedViewMasks = nullptr;
        IR_VULKAN_CHECK(device.logger(), vkCreateRenderPass2(device.handle(), &render_pass_info, nullptr, &render_pass->_handle));
        IR_LOG_INFO(device.logger(), "render pass initialized {}", fmt::ptr(render_pass->_handle));

        render_pass->_info = info;
        render_pass->_device = device.as_intrusive_ptr();
        return render_pass;
    }

    auto render_pass_t::handle() const noexcept -> VkRenderPass {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto render_pass_t::attachments() const noexcept -> std::span<const attachment_info_t> {
        IR_PROFILE_SCOPED();
        return _info.attachments;
    }

    auto render_pass_t::attachment(uint32 index) const noexcept -> const attachment_info_t& {
        IR_PROFILE_SCOPED();
        return _info.attachments[index];
    }

    auto render_pass_t::subpasses() const noexcept -> std::span<const subpass_info_t> {
        IR_PROFILE_SCOPED();
        return _info.subpasses;
    }

    auto render_pass_t::dependencies() const noexcept -> std::span<const subpass_dependency_info_t> {
        IR_PROFILE_SCOPED();
        return _info.dependencies;
    }

    auto render_pass_t::info() const noexcept -> const render_pass_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto render_pass_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }
}
