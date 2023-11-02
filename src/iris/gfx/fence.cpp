#include <iris/gfx/fence.hpp>
#include <iris/gfx/device.hpp>

namespace ir {
    fence_t::fence_t() noexcept = default;

    fence_t::~fence_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroyFence(device().handle(), _handle, nullptr);
        IR_LOG_INFO(device().logger(), "fence {} destroyed", fmt::ptr(_handle));
    }

    auto fence_t::make(
        const device_t& device,
        bool signaled,
        const std::string& name
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto fence = arc_ptr<self>(new self());
        auto fence_info = VkFenceCreateInfo();
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.pNext = nullptr;
        fence_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlagBits();
        IR_VULKAN_CHECK(device.logger(), vkCreateFence(device.handle(), &fence_info, nullptr, &fence->_handle));
        IR_LOG_INFO(device.logger(), "fence {} created", fmt::ptr(fence->_handle));
        fence->_device = device.as_intrusive_ptr();

        if (!name.empty()) {
            device.set_debug_name({
                .type = VK_OBJECT_TYPE_FENCE,
                .handle = reinterpret_cast<uint64>(fence->_handle),
                .name = name.c_str(),
            });
        }
        return fence;
    }

    auto fence_t::make(
        const device_t& device,
        uint32 count,
        bool signaled,
        const std::string& name
    ) noexcept -> std::vector<arc_ptr<self>> {
        IR_PROFILE_SCOPED();
        auto fences = std::vector<arc_ptr<self>>(count);
        for (auto i = 0_u32; i < count; ++i) {
            fences[i] = make(device, signaled, std::format("{}_{}", name, i));
        }
        return fences;
    }

    auto fence_t::handle() const noexcept -> VkFence {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto fence_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }

    auto fence_t::is_ready() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return vkGetFenceStatus(device().handle(), _handle) == VK_SUCCESS;
    }

    auto fence_t::wait(uint64 timeout) const noexcept -> void {
        IR_PROFILE_SCOPED();
        IR_VULKAN_CHECK(device().logger(), vkWaitForFences(device().handle(), 1, &_handle, true, timeout));
    }

    auto fence_t::reset() const noexcept -> void {
        IR_PROFILE_SCOPED();
        IR_VULKAN_CHECK(device().logger(), vkResetFences(device().handle(), 1, &_handle));
    }
}
