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

        for (const auto& [semaphore, stage] : info.wait_semaphores) {
            wait_semaphore_info.emplace_back(VkSemaphoreSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = semaphore.get().handle(),
                .stageMask = as_enum_counterpart(stage)
            });
        }

        for (const auto& [semaphore, stage] : info.signal_semaphores) {
            signal_semaphore_info.emplace_back(VkSemaphoreSubmitInfo {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = semaphore.get().handle(),
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
        command_buffer.as_ref().begin();
        record(*command_buffer);
        command_buffer.as_ref().end();
        submit({
            .command_buffers = { std::cref(*command_buffer) },
            .wait_semaphores = {},
            .signal_semaphores = {},
        }, fence.get());
        fence.as_const_ref().wait();
    }

    auto queue_t::present(const queue_present_info_t& info) noexcept -> void {
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
        IR_VULKAN_CHECK(_device.get().logger(), vkQueuePresentKHR(_handle, &present_info));
    }
}
