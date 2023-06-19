#include <iris/gfx/device.hpp>
#include <iris/gfx/command_pool.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/image.hpp>

namespace ir {
    command_buffer_t::command_buffer_t() noexcept = default;

    command_buffer_t::~command_buffer_t() noexcept {
        IR_PROFILE_SCOPED();
        vkFreeCommandBuffers(_pool.as_const_ref().device().handle(), _pool->handle(), 1, &_handle);
        IR_LOG_INFO(_pool.as_const_ref().device().logger(), "command buffer {} destroyed", fmt::ptr(_handle));
    }

    auto command_buffer_t::make(
        const command_pool_t& pool,
        const command_buffer_create_info_t& info
    ) noexcept -> arc_ptr<self> {
        auto command_buffer = ir::arc_ptr<self>(new self());
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

    auto command_buffer_t::begin() const noexcept -> void {
        IR_PROFILE_SCOPED();
        auto command_buffer_begin_info = VkCommandBufferBeginInfo();
        command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        command_buffer_begin_info.pNext = nullptr;
        command_buffer_begin_info.flags = 0;
        command_buffer_begin_info.pInheritanceInfo = nullptr;
        IR_VULKAN_CHECK(_pool.as_const_ref().device().logger(), vkBeginCommandBuffer(_handle, &command_buffer_begin_info));
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
        if (barrier.level != level_ignored) {
            image_barrier.subresourceRange.baseMipLevel = barrier.level;
            image_barrier.subresourceRange.levelCount = barrier.level_count;
        } else {
            image_barrier.subresourceRange.baseMipLevel = 0;
            image_barrier.subresourceRange.levelCount = image.levels();
        }
        if (barrier.layer != layer_ignored) {
            image_barrier.subresourceRange.baseArrayLayer = barrier.layer;
            image_barrier.subresourceRange.layerCount = barrier.layer_count;
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
        IR_VULKAN_CHECK(_pool.as_const_ref().device().logger(), vkEndCommandBuffer(_handle));
    }
}
