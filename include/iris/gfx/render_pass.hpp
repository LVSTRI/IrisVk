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
    struct attachment_layout_t {
        image_layout_t initial = image_layout_t::e_undefined;
        image_layout_t final = image_layout_t::e_undefined;
    };

    struct attachment_info_t {
        attachment_layout_t layout = {};
        resource_format_t format = {};
        sample_count_t samples = sample_count_t::e_1;
        attachment_load_op_t load_op = attachment_load_op_t::e_dont_care;
        attachment_store_op_t store_op = attachment_store_op_t::e_dont_care;
        attachment_load_op_t stencil_load_op = attachment_load_op_t::e_dont_care;
        attachment_store_op_t stencil_store_op = attachment_store_op_t::e_dont_care;
    };

    struct subpass_info_t {
        std::vector<uint32> color_attachments = {};
        std::optional<uint32> depth_stencil_attachment = std::nullopt;
        std::vector<uint32> input_attachments = {};
        std::vector<uint32> preserve_attachments = {};
    };

    struct subpass_dependency_info_t {
        uint32 source = 0;
        uint32 dest = 0;
        pipeline_stage_t source_stage = pipeline_stage_t::e_none;
        pipeline_stage_t dest_stage = pipeline_stage_t::e_none;
        resource_access_t source_access = resource_access_t::e_none;
        resource_access_t dest_access = resource_access_t::e_none;
    };

    struct render_pass_create_info_t {
        std::vector<attachment_info_t> attachments;
        std::vector<subpass_info_t> subpasses;
        std::vector<subpass_dependency_info_t> dependencies;
    };

    class render_pass_t : public enable_intrusive_refcount_t<render_pass_t> {
    public:
        using self = render_pass_t;

        render_pass_t() noexcept;
        ~render_pass_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device, const render_pass_create_info_t& info) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkRenderPass;
        IR_NODISCARD auto attachments() const noexcept -> std::span<const attachment_info_t>;
        IR_NODISCARD auto attachment(uint32 index) const noexcept -> const attachment_info_t&;
        IR_NODISCARD auto subpasses() const noexcept -> std::span<const subpass_info_t>;
        IR_NODISCARD auto dependencies() const noexcept -> std::span<const subpass_dependency_info_t>;
        IR_NODISCARD auto info() const noexcept -> const render_pass_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;
    private:
        VkRenderPass _handle = {};

        render_pass_create_info_t _info = {};
        arc_ptr<const device_t> _device;
    };
}

