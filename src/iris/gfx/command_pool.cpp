#include <iris/gfx/device.hpp>
#include <iris/gfx/queue.hpp>
#include <iris/gfx/command_pool.hpp>

namespace ir {
    command_pool_t::command_pool_t(const device_t& device) noexcept : _device(std::cref(device)) {
        IR_PROFILE_SCOPED();
    }

    command_pool_t::~command_pool_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroyCommandPool(device().handle(), _handle, nullptr);
        IR_LOG_INFO(device().logger(), "command pool {} destroyed", fmt::ptr(_handle));
    }

    auto command_pool_t::make(const device_t& device, const command_pool_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto command_pool = arc_ptr<self>(new self(device));
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
        if (!info.name.empty()) {
            device.set_debug_name({
                .type = VK_OBJECT_TYPE_COMMAND_POOL,
                .handle = reinterpret_cast<uint64>(command_pool->_handle),
                .name = info.name.c_str()
            });
        }
        return command_pool;
    }

    auto command_pool_t::make(
        const device_t& device,
        uint32 count,
        const command_pool_create_info_t& info
    ) noexcept -> std::vector<arc_ptr<self>> {
        IR_PROFILE_SCOPED();
        auto command_pools = std::vector<arc_ptr<self>>(count);
        for (auto i = 0_u32; i < count; i++) {
            command_pools[i] = make(device, {
                .name = fmt::format("{}_{}", info.name, i),
                .queue = info.queue,
                .flags = info.flags
            });
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
        return _device.get();
    }

    auto command_pool_t::reset() const noexcept -> void {
        IR_PROFILE_SCOPED();
        IR_VULKAN_CHECK(device().logger(), vkResetCommandPool(device().handle(), _handle, 0));
    }
}
