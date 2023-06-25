#include <iris/gfx/device.hpp>
#include <iris/gfx/command_pool.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/pipeline.hpp>
#include <iris/gfx/descriptor_set.hpp>
#include <iris/gfx/clear_value.hpp>
#include <iris/gfx/render_pass.hpp>
#include <iris/gfx/framebuffer.hpp>
#include <iris/gfx/image.hpp>

namespace ir {
    command_buffer_t::command_buffer_t() noexcept = default;

    command_buffer_t::~command_buffer_t() noexcept {
        IR_PROFILE_SCOPED();
        vkFreeCommandBuffers(pool().device().handle(), pool().handle(), 1, &_handle);
        IR_LOG_INFO(pool().device().logger(), "command buffer {} destroyed", fmt::ptr(_handle));
    }

    auto command_buffer_t::make(
        const command_pool_t& pool,
        const command_buffer_create_info_t& info
    ) noexcept -> arc_ptr<self> {
        auto command_buffer = arc_ptr<self>(new self());
        auto command_buffer_info = VkCommandBufferAllocateInfo();
        command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_info.pNext = nullptr;
        command_buffer_info.commandPool = pool.handle();
        command_buffer_info.level = info.primary ?
            VK_COMMAND_BUFFER_LEVEL_PRIMARY :
            VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        command_buffer_info.commandBufferCount = 1;
        IR_VULKAN_CHECK(pool.device().logger(), vkAllocateCommandBuffers(pool.device().handle(), &command_buffer_info, &command_buffer->_handle));
        IR_LOG_INFO(pool.device().logger(), "command buffer {} initialized", fmt::ptr(command_buffer->_handle));

        command_buffer->_info = info;
        command_buffer->_pool = pool.as_intrusive_ptr();
        return command_buffer;
    }

    auto command_buffer_t::make(
        const command_pool_t& pool,
        uint32 count,
        const command_buffer_create_info_t& info
    ) noexcept -> std::vector<arc_ptr<self>> {
        IR_PROFILE_SCOPED();
        auto command_buffer_handles = std::vector<VkCommandBuffer>(count);
        auto command_buffer_info = VkCommandBufferAllocateInfo();
        command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_info.pNext = nullptr;
        command_buffer_info.commandPool = pool.handle();
        command_buffer_info.level = info.primary ?
            VK_COMMAND_BUFFER_LEVEL_PRIMARY :
            VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        command_buffer_info.commandBufferCount = count;
        IR_VULKAN_CHECK(
            pool.device().logger(),
            vkAllocateCommandBuffers(pool.device().handle(), &command_buffer_info, command_buffer_handles.data()));

        auto command_buffers = std::vector<arc_ptr<self>>(count);
        for (auto i = 0_u32; i < count; ++i) {
            command_buffers[i]->_handle = command_buffer_handles[i];
            command_buffers[i]->_info = info;
            command_buffers[i]->_pool = pool.as_intrusive_ptr();
        }
        return command_buffers;
    }

    auto command_buffer_t::handle() const noexcept -> VkCommandBuffer {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto command_buffer_t::info() const noexcept -> const command_buffer_create_info_t& {
        return _info;
    }

    auto command_buffer_t::pool() const noexcept -> const command_pool_t& {
        IR_PROFILE_SCOPED();
        return *_pool;
    }

    auto command_buffer_t::begin() noexcept -> void {
        IR_PROFILE_SCOPED();
        auto command_buffer_begin_info = VkCommandBufferBeginInfo();
        command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        command_buffer_begin_info.pNext = nullptr;
        command_buffer_begin_info.flags = 0;
        command_buffer_begin_info.pInheritanceInfo = nullptr;
        IR_VULKAN_CHECK(pool().device().logger(), vkBeginCommandBuffer(_handle, &command_buffer_begin_info));
    }

    auto command_buffer_t::begin_render_pass(const framebuffer_t& framebuffer, const std::vector<clear_value_t>& clears) noexcept -> void {
        IR_PROFILE_SCOPED();
        _state.framebuffer = &framebuffer;

        auto clear_values = std::vector<VkClearValue>(clears.size());
        for (auto i = 0_u32; i < clears.size(); ++i) {
            switch (clears[i].type()) {
                case clear_value_type_t::e_color:
                    std::memcpy(&clear_values[i].color, as_const_ptr(clears[i].color()), sizeof(clear_values[i].color));
                    break;

                case clear_value_type_t::e_depth:
                    std::memcpy(&clear_values[i].depthStencil, as_const_ptr(clears[i].depth()), sizeof(clear_values[i].depthStencil));
                    break;

                default: break;
            }
        }

        auto render_pass_begin_info = VkRenderPassBeginInfo();
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.pNext = nullptr;
        render_pass_begin_info.renderPass = framebuffer.render_pass().handle();
        render_pass_begin_info.framebuffer = framebuffer.handle();
        render_pass_begin_info.renderArea.offset = { 0, 0 };
        render_pass_begin_info.renderArea.extent = {
            framebuffer.width(),
            framebuffer.height()
        };
        render_pass_begin_info.clearValueCount = clear_values.size();
        render_pass_begin_info.pClearValues = clear_values.data();
        vkCmdBeginRenderPass(_handle, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    }

    auto command_buffer_t::set_viewport(const viewport_t& viewport) const noexcept -> void {
        IR_PROFILE_SCOPED();
        auto v = VkViewport();
        v.x = viewport.x;
        v.y = viewport.height - viewport.y;
        v.width = viewport.width;
        v.height = viewport.y - viewport.height;
        v.minDepth = 0.0f;
        v.maxDepth = 1.0f;
        vkCmdSetViewport(_handle, 0, 1, &v);
    }

    auto command_buffer_t::set_scissor(const scissor_t& scissor) const noexcept -> void {
        IR_PROFILE_SCOPED();
        auto s = VkRect2D();
        s.offset = { scissor.x, scissor.y };
        s.extent = { scissor.width, scissor.height };
        vkCmdSetScissor(_handle, 0, 1, &s);
    }

    auto command_buffer_t::bind_pipeline(const pipeline_t& pipeline) noexcept -> void {
        IR_PROFILE_SCOPED();
        _state.pipeline = &pipeline;
        const auto bind_point = [&]() -> VkPipelineBindPoint {
            switch (pipeline.type()) {
                case pipeline_type_t::e_graphics: return VK_PIPELINE_BIND_POINT_GRAPHICS;
                case pipeline_type_t::e_compute: return VK_PIPELINE_BIND_POINT_COMPUTE;
                case pipeline_type_t::e_ray_tracing: return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
            }
        }();
        vkCmdBindPipeline(_handle, bind_point, pipeline.handle());
    }

    auto command_buffer_t::bind_descriptor_set(const descriptor_set_t& set) noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto bind_point = [this]() -> VkPipelineBindPoint {
            switch (_state.pipeline->type()) {
                case pipeline_type_t::e_graphics: return VK_PIPELINE_BIND_POINT_GRAPHICS;
                case pipeline_type_t::e_compute: return VK_PIPELINE_BIND_POINT_COMPUTE;
                case pipeline_type_t::e_ray_tracing: return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
            }
        }();
        const auto handle = set.handle();
        vkCmdBindDescriptorSets(
            _handle,
            bind_point,
            _state.pipeline->layout(),
            set.layout().index(),
            1,
            &handle,
            0,
            nullptr);
    }

    auto command_buffer_t::bind_vertex_buffer(const buffer_info_t& buffer) const noexcept -> void {
        IR_PROFILE_SCOPED();
        vkCmdBindVertexBuffers(_handle, 0, 1, &buffer.handle, &buffer.offset);
    }

    auto command_buffer_t::bind_index_buffer(const buffer_info_t& buffer, index_type_t type) const noexcept -> void {
        IR_PROFILE_SCOPED();
        vkCmdBindIndexBuffer(_handle, buffer.handle, buffer.offset, static_cast<VkIndexType>(as_enum_counterpart(type)));
    }

    auto command_buffer_t::push_constants(shader_stage_t stage, uint32 offset, uint64 size, const void* data) const noexcept -> void {
        IR_PROFILE_SCOPED();
        vkCmdPushConstants(_handle, _state.pipeline->layout(), as_enum_counterpart(stage), offset, size, data);
    }

    auto command_buffer_t::draw(uint32 vertices, uint32 instances, uint32 first_vertex, uint32 first_instance) const noexcept -> void {
        IR_PROFILE_SCOPED();
        vkCmdDraw(_handle, vertices, instances, first_vertex, first_instance);
    }

    auto command_buffer_t::draw_indexed(uint32 indices, uint32 instances, uint32 first_index, int32 vertex_offset, uint32 first_instance) const noexcept -> void {
        IR_PROFILE_SCOPED();
        vkCmdDrawIndexed(_handle, indices, instances, first_index, vertex_offset, first_instance);
    }

    auto command_buffer_t::draw_mesh_tasks(uint32 x, uint32 y, uint32 z) const noexcept -> void {
        IR_PROFILE_SCOPED();
        vkCmdDrawMeshTasksEXT(_handle, x, y, z);
    }

    auto command_buffer_t::end_render_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        _state.framebuffer = nullptr;
        vkCmdEndRenderPass(_handle);
    }

    auto command_buffer_t::clear_image(const image_t& image, const clear_value_t& clear, const image_subresource_t& subresource) const noexcept -> void {
        IR_PROFILE_SCOPED();
        IR_ASSERT(clear.type() == clear_value_type_t::e_color, "clear_image: color only");
        auto clear_value = VkClearColorValue();
        std::memcpy(&clear_value, as_const_ptr(clear.color()), sizeof(clear.color()));
        auto subresource_range = VkImageSubresourceRange();
        subresource_range.aspectMask = as_enum_counterpart(image.view().aspect());
        if (subresource.level != level_ignored) {
            subresource_range.baseMipLevel = subresource.level;
            subresource_range.levelCount = subresource.level_count;
        } else {
            subresource_range.baseMipLevel = 0;
            subresource_range.levelCount = image.levels();
        }
        if (subresource.layer != layer_ignored) {
            subresource_range.baseArrayLayer = subresource.layer;
            subresource_range.layerCount = subresource.layer_count;
        } else {
            subresource_range.baseArrayLayer = 0;
            subresource_range.layerCount = image.layers();
        }

        vkCmdClearColorImage(
            _handle,
            image.handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &clear_value,
            1,
            &subresource_range);
    }

    auto command_buffer_t::copy_image(const image_t& source, const image_t& dest, const image_copy_t& copy) const noexcept -> void {
        IR_PROFILE_SCOPED();
        auto copy_region = VkImageCopy();
        copy_region.srcSubresource.aspectMask = as_enum_counterpart(source.view().aspect());
        if (copy.source_subresource.level != level_ignored) {
            copy_region.srcSubresource.mipLevel = copy.source_subresource.level;
        }
        if (copy.source_subresource.layer != layer_ignored) {
            copy_region.srcSubresource.baseArrayLayer = copy.source_subresource.layer;
            copy_region.srcSubresource.layerCount = copy.source_subresource.layer_count;
        } else {
            copy_region.srcSubresource.baseArrayLayer = 0;
            copy_region.srcSubresource.layerCount = source.layers();
        }

        copy_region.dstSubresource.aspectMask = as_enum_counterpart(dest.view().aspect());
        if (copy.dest_subresource.level != level_ignored) {
            copy_region.dstSubresource.mipLevel = copy.dest_subresource.level;
        }
        if (copy.dest_subresource.layer != layer_ignored) {
            copy_region.dstSubresource.baseArrayLayer = copy.dest_subresource.layer;
            copy_region.dstSubresource.layerCount = copy.dest_subresource.layer_count;
        } else {
            copy_region.dstSubresource.baseArrayLayer = 0;
            copy_region.dstSubresource.layerCount = dest.layers();
        }

        if (copy.source_offset == ignored_offset_3d) {
            copy_region.srcOffset = { 0, 0, 0 };
        } else {
            copy_region.srcOffset = {
                copy.source_offset.x,
                copy.source_offset.y,
                copy.source_offset.z
            };
        }
        if (copy.dest_offset == ignored_offset_3d) {
            copy_region.dstOffset = { 0, 0, 0 };
        } else {
            copy_region.dstOffset = {
                copy.dest_offset.x,
                copy.dest_offset.y,
                copy.dest_offset.z
            };
        }
        if (copy.extent == ignored_extent_3d) {
            copy_region.extent = {
                source.width(),
                source.height(),
                // TODO
                1
            };
        } else {
            copy_region.extent = {
                copy.extent.width,
                copy.extent.height,
                copy.extent.depth
            };
        }

        vkCmdCopyImage(
            _handle,
            source.handle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dest.handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copy_region
        );
    }

    auto command_buffer_t::copy_buffer(
        const buffer_info_t& source,
        const buffer_info_t& dest,
        const buffer_copy_t& copy
    ) const noexcept -> void {
        IR_PROFILE_SCOPED();
        auto copy_region = VkBufferCopy();
        copy_region.srcOffset = copy.source_offset;
        copy_region.dstOffset = copy.dest_offset;
        copy_region.size = source.size;
        vkCmdCopyBuffer(_handle, source.handle, dest.handle, 1, &copy_region);
    }

    auto command_buffer_t::memory_barrier(const memory_barrier_t& barrier) const noexcept -> void {
        IR_PROFILE_SCOPED();
        auto memory_barrier = VkMemoryBarrier2();
        memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memory_barrier.pNext = nullptr;
        memory_barrier.srcStageMask = as_enum_counterpart(barrier.source_stage);
        memory_barrier.srcAccessMask = as_enum_counterpart(barrier.source_access);
        memory_barrier.dstStageMask = as_enum_counterpart(barrier.dest_stage);
        memory_barrier.dstAccessMask = as_enum_counterpart(barrier.dest_access);

        auto dependency_info = VkDependencyInfo();
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.pNext = nullptr;
        dependency_info.dependencyFlags = {};
        dependency_info.memoryBarrierCount = 1;
        dependency_info.pMemoryBarriers = &memory_barrier;
        vkCmdPipelineBarrier2(_handle, &dependency_info);
    }

    auto command_buffer_t::image_barrier(const image_memory_barrier_t& barrier) const noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& image = barrier.image.get();
        auto image_barrier = VkImageMemoryBarrier2();
        image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        image_barrier.pNext = nullptr;
        image_barrier.srcStageMask = as_enum_counterpart(barrier.source_stage);
        image_barrier.srcAccessMask = as_enum_counterpart(barrier.source_access);
        image_barrier.dstStageMask = as_enum_counterpart(barrier.dest_stage);
        image_barrier.dstAccessMask = as_enum_counterpart(barrier.dest_access);
        image_barrier.oldLayout = as_enum_counterpart(barrier.old_layout);
        image_barrier.newLayout = as_enum_counterpart(barrier.new_layout);
        image_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        image_barrier.image = image.handle();
        image_barrier.subresourceRange.aspectMask = as_enum_counterpart(image.view().aspect());
        if (barrier.subresource.level != level_ignored) {
            image_barrier.subresourceRange.baseMipLevel = barrier.subresource.level;
            image_barrier.subresourceRange.levelCount = barrier.subresource.level_count;
        } else {
            image_barrier.subresourceRange.baseMipLevel = 0;
            image_barrier.subresourceRange.levelCount = image.levels();
        }
        if (barrier.subresource.layer != layer_ignored) {
            image_barrier.subresourceRange.baseArrayLayer = barrier.subresource.layer;
            image_barrier.subresourceRange.layerCount = barrier.subresource.layer_count;
        } else {
            image_barrier.subresourceRange.baseArrayLayer = 0;
            image_barrier.subresourceRange.layerCount = image.layers();
        }

        auto dependency_info = VkDependencyInfo();
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.pNext = nullptr;
        dependency_info.dependencyFlags = {};
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &image_barrier;
        vkCmdPipelineBarrier2(_handle, &dependency_info);
    }

    auto command_buffer_t::end() const noexcept -> void {
        IR_PROFILE_SCOPED();
        IR_VULKAN_CHECK(pool().device().logger(), vkEndCommandBuffer(_handle));
    }
}
