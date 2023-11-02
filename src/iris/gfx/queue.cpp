#include <iris/core/utilities.hpp>

#include <iris/gfx/device.hpp>
#include <iris/gfx/fence.hpp>
#include <iris/gfx/semaphore.hpp>
#include <iris/gfx/command_pool.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/swapchain.hpp>
#include <iris/gfx/queue.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace ir {
    template <>
    constexpr auto internal_enum_as_string(queue_type_t type) noexcept -> std::string_view {
        switch (type) {
            case queue_type_t::e_graphics: return "graphics";
            case queue_type_t::e_compute: return "compute";
            case queue_type_t::e_transfer: return "transfer";
        }
        IR_UNREACHABLE();
    }

    queue_t::queue_t(const device_t& device) noexcept : _device(std::cref(device)) {
        IR_PROFILE_SCOPED();
    }

    queue_t::~queue_t() noexcept {
        IR_PROFILE_SCOPED();
        IR_LOG_INFO(_logger, "queue destroyed");
    }

    auto queue_t::make(const device_t& device, const queue_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto queue = arc_ptr<self>(new self(device));
        auto logger = spdlog::stdout_color_mt(internal_enum_as_string(info.type).data());
        IR_LOG_INFO(logger, "queue initialized (family: {}, index: {})", info.family.family, info.family.index);

        queue->_handle = device.fetch_queue(info.family);
        queue->_info = info;
        queue->_logger = std::move(logger);
        if (!info.name.empty()) {
            device.set_debug_name({
                .type = VK_OBJECT_TYPE_QUEUE,
                .handle = reinterpret_cast<uint64>(queue->_handle),
                .name = info.name.c_str()
            });
        }
        return queue;
    }

    auto queue_t::handle() const noexcept -> VkQueue {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto queue_t::family() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _info.family.family;
    }

    auto queue_t::index() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _info.family.index;
    }

    auto queue_t::type() const noexcept -> queue_type_t {
        IR_PROFILE_SCOPED();
        return _info.type;
    }

    auto queue_t::transient_pool(uint32 index) noexcept -> command_pool_t& {
        IR_PROFILE_SCOPED();
        if (_transient_pools.empty()) {
            // index = 0 => main thread
            _transient_pools = command_pool_t::make(_device, std::thread::hardware_concurrency() + 1, {
                .name = "transient_command_pool",
                .queue = type(),
                .flags = command_pool_flag_t::e_transient,
            });
        }
        return *_transient_pools[index];
    }

    auto queue_t::info() const noexcept -> const queue_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto queue_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return _device.get();
    }

    auto queue_t::logger() const noexcept -> spdlog::logger& {
        IR_PROFILE_SCOPED();
        return *_logger;
    }

    auto queue_t::submit(const queue_submit_info_t& info, const fence_t* fence) noexcept -> void {
        IR_PROFILE_SCOPED();
        auto wait_semaphore_info = std::vector<VkSemaphoreSubmitInfo>();
        wait_semaphore_info.reserve(info.wait_semaphores.size());
        auto signal_semaphore_info = std::vector<VkSemaphoreSubmitInfo>();
        signal_semaphore_info.reserve(info.signal_semaphores.size());
        auto command_buffer_info = std::vector<VkCommandBufferSubmitInfo>();
        command_buffer_info.reserve(info.command_buffers.size());

        for (const auto& [semaphore, stage, value] : info.wait_semaphores) {
            wait_semaphore_info.emplace_back(VkSemaphoreSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = semaphore.get().handle(),
                .value = value == -1_u64 ? 0 : value,
                .stageMask = as_enum_counterpart(stage)
            });
        }

        for (const auto& [semaphore, stage, value] : info.signal_semaphores) {
            signal_semaphore_info.emplace_back(VkSemaphoreSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = semaphore.get().handle(),
                .value = value == -1_u64 ? 0 : value,
                .stageMask = as_enum_counterpart(stage)
            });
        }

        for (const auto& command_buffer : info.command_buffers) {
            command_buffer_info.emplace_back(VkCommandBufferSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext = nullptr,
                .commandBuffer = command_buffer.get().handle()
            });
        }

        auto submit_info = VkSubmitInfo2();
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit_info.pNext = nullptr;
        submit_info.flags = {};
        submit_info.waitSemaphoreInfoCount = wait_semaphore_info.size();
        submit_info.pWaitSemaphoreInfos = wait_semaphore_info.data();
        submit_info.commandBufferInfoCount = command_buffer_info.size();
        submit_info.pCommandBufferInfos = command_buffer_info.data();
        submit_info.signalSemaphoreInfoCount = signal_semaphore_info.size();
        submit_info.pSignalSemaphoreInfos = signal_semaphore_info.data();
        auto guard = std::lock_guard(_lock);
        IR_VULKAN_CHECK(_device.get().logger(), vkQueueSubmit2(_handle, 1, &submit_info, fence ? fence->handle() : nullptr));
    }

    auto queue_t::submit(const std::function<void(command_buffer_t&)>& record) noexcept -> void {
        IR_PROFILE_SCOPED();
        auto command_buffer = command_buffer_t::make(transient_pool(0), {});
        auto fence = fence_t::make(device(), false);
        command_buffer->begin();
        record(*command_buffer);
        command_buffer->end();
        submit({
            .command_buffers = { std::cref(*command_buffer) },
            .wait_semaphores = {},
            .signal_semaphores = {},
        }, fence.get());
        fence->wait();
    }

    auto queue_t::present(const queue_present_info_t& info) noexcept -> bool {
        IR_PROFILE_SCOPED();
        auto wait_semaphore_info = std::vector<VkSemaphore>();
        wait_semaphore_info.reserve(info.wait_semaphores.size());
        for (const auto& semaphore : info.wait_semaphores) {
            wait_semaphore_info.emplace_back(semaphore.get().handle());
        }

        const auto swapchain = info.swapchain.get().handle();
        auto present_info = VkPresentInfoKHR();
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.pNext = nullptr;
        present_info.waitSemaphoreCount = wait_semaphore_info.size();
        present_info.pWaitSemaphores = wait_semaphore_info.data();
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain;
        present_info.pImageIndices = &info.image;
        present_info.pResults = nullptr;
        auto guard = std::lock_guard(_lock);
        const auto result = vkQueuePresentKHR(_handle, &present_info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR ||
            result == VK_ERROR_SURFACE_LOST_KHR ||
            result == VK_SUBOPTIMAL_KHR
        ) {
            return true;
        }
        if (result != VK_SUCCESS) {
            IR_VULKAN_CHECK(_device.get().logger(), result);
            IR_UNREACHABLE();
        }
        return false;
    }

    auto queue_t::bind_sparse(const queue_bind_sparse_info_t& info, const fence_t* fence) noexcept -> void {
        IR_PROFILE_SCOPED();
        auto semaphore_wait_values = std::vector<uint64>(info.wait_semaphores.size());
        auto semaphore_signal_values = std::vector<uint64>(info.signal_semaphores.size());

        auto wait_semaphore_info = std::vector<VkSemaphore>();
        wait_semaphore_info.reserve(info.wait_semaphores.size());
        for (auto index = 0_u32; const auto& [semaphore, stage, value] : info.wait_semaphores) {
            wait_semaphore_info.emplace_back(semaphore.get().handle());
            semaphore_wait_values[index] = value == -1_u64 ? 0 : value;
        }

        auto signal_semaphore_info = std::vector<VkSemaphore>();
        signal_semaphore_info.reserve(info.signal_semaphores.size());
        for (auto index = 0_u32; const auto& [semaphore, stage, value] : info.signal_semaphores) {
            signal_semaphore_info.emplace_back(semaphore.get().handle());
            semaphore_signal_values[index] = value == -1_u64 ? 0 : value;
        }

        auto image_bind_info = std::vector<VkSparseImageMemoryBindInfo>();
        image_bind_info.reserve(info.image_binds.size());
        auto image_memory_bind_infos = std::vector<std::vector<VkSparseImageMemoryBind>>();
        for (const auto& image_bind : info.image_binds) {
            const auto& image = image_bind.image.get();
            auto& image_memory_bind_info = image_memory_bind_infos.emplace_back(std::vector<VkSparseImageMemoryBind>());
            image_memory_bind_info.reserve(image_bind.bindings.size());
            for (const auto& sparse_bind : image_bind.bindings) {
                auto bind = VkSparseImageMemoryBind();
                bind.subresource.aspectMask = as_enum_counterpart(image.view().aspect());
                bind.subresource.mipLevel = sparse_bind.level;
                bind.subresource.arrayLayer = sparse_bind.layer;
                bind.offset.x = sparse_bind.offset.x;
                bind.offset.y = sparse_bind.offset.y;
                bind.offset.z = sparse_bind.offset.z;
                bind.extent.width = sparse_bind.extent.width;
                bind.extent.height = sparse_bind.extent.height;
                bind.extent.depth = sparse_bind.extent.depth;
                bind.memory = sparse_bind.buffer.memory;
                bind.memoryOffset = sparse_bind.buffer.offset;
                image_memory_bind_info.emplace_back(bind);
            }

            auto bind_info = VkSparseImageMemoryBindInfo();
            bind_info.image = image.handle();
            bind_info.bindCount = image_memory_bind_info.size();
            bind_info.pBinds = image_memory_bind_info.data();
            image_bind_info.emplace_back(bind_info);
        }

        auto semaphore_submit_info = VkTimelineSemaphoreSubmitInfo();
        semaphore_submit_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        semaphore_submit_info.waitSemaphoreValueCount = semaphore_wait_values.size();
        semaphore_submit_info.pWaitSemaphoreValues = semaphore_wait_values.data();
        semaphore_submit_info.signalSemaphoreValueCount = semaphore_signal_values.size();
        semaphore_submit_info.pSignalSemaphoreValues = semaphore_signal_values.data();

        auto bind_sparse_info = VkBindSparseInfo();
        bind_sparse_info.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
        bind_sparse_info.pNext = &semaphore_submit_info;
        bind_sparse_info.waitSemaphoreCount = wait_semaphore_info.size();
        bind_sparse_info.pWaitSemaphores = wait_semaphore_info.data();
        // TODO: sparse binding buffers / opaque memory binds
        bind_sparse_info.bufferBindCount = 0;
        bind_sparse_info.pBufferBinds = nullptr;
        bind_sparse_info.imageBindCount = image_bind_info.size();
        bind_sparse_info.pImageBinds = image_bind_info.data();
        bind_sparse_info.imageOpaqueBindCount = 0;
        bind_sparse_info.pImageOpaqueBinds = nullptr;
        bind_sparse_info.signalSemaphoreCount = signal_semaphore_info.size();
        bind_sparse_info.pSignalSemaphores = signal_semaphore_info.data();
        auto guard = std::lock_guard(_lock);
        IR_VULKAN_CHECK(_device.get().logger(), vkQueueBindSparse(_handle, 1, &bind_sparse_info, fence ? fence->handle() : nullptr));
    }

    auto queue_t::wait_idle() noexcept -> void {
        IR_PROFILE_SCOPED();
        auto guard = std::lock_guard(_lock);
        IR_VULKAN_CHECK(_device.get().logger(), vkQueueWaitIdle(_handle));
    }
}
