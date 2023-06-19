#include <iris/gfx/device.hpp>
#include <iris/gfx/queue.hpp>
#include <iris/gfx/command_pool.hpp>

namespace ir {
    command_pool_t::command_pool_t() noexcept = default;

    command_pool_t::~command_pool_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroyCommandPool(_device.as_const_ref().handle(), _handle, nullptr);
        IR_LOG_INFO(_device.as_const_ref().logger(), "command pool {} destroyed", fmt::ptr(_handle));
    }

    auto command_pool_t::make(const device_t& device, const command_pool_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto command_pool = arc_ptr<self>(new self());
        auto command_pool_info = VkCommandPoolCreateInfo();
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.pNext = nullptr;
        command_pool_info.flags = as_enum_counterpart(info.flags);
        command_pool_info.queueFamilyIndex = [&]() -> uint32 {
            switch (info.queue) {
                case queue_type_t::e_graphics: return device.graphics_queue().family();
                case queue_type_t::e_compute: return device.compute_queue().family();
                case queue_type_t::e_transfer: return device.transfer_queue().family();
            }
            IR_UNREACHABLE();
        }();
        IR_VULKAN_CHECK(device.logger(), vkCreateCommandPool(device.handle(), &command_pool_info, nullptr, &command_pool->_handle));
        IR_LOG_INFO(device.logger(), "command pool {} initialized (family: {})",
            fmt::ptr(command_pool->_handle),
            command_pool_info.queueFamilyIndex);

        command_pool->_info = info;
        command_pool->_device = device.as_intrusive_ptr();
        return command_pool;
    }

    auto command_pool_t::make(
        const device_t& device,
        uint32 count,
        const command_pool_create_info_t& info
    ) noexcept -> std::vector<arc_ptr<self>> {
        IR_PROFILE_SCOPED();
        auto command_pools = std::vector<arc_ptr < self>>(count);
        for (auto& command_pool : command_pools) {
            command_pool = make(device, info);
        }
        return command_pools;
    }

    auto command_pool_t::handle() const noexcept -> VkCommandPool {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto command_pool_t::info() const noexcept -> const command_pool_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto command_pool_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }

    auto command_pool_t::reset() const noexcept -> void {
        IR_PROFILE_SCOPED();
        IR_VULKAN_CHECK(_device.as_const_ref().logger(), vkResetCommandPool(_device.as_const_ref().handle(), _handle, 0));
    }
}
