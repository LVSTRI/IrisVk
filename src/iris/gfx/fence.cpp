#include <iris/gfx/fence.hpp>
#include <iris/gfx/device.hpp>

namespace ir {
    fence_t::fence_t() noexcept = default;

    fence_t::~fence_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroyFence(_device.as_const_ref().handle(), _handle, nullptr);
        IR_LOG_INFO(_device.as_const_ref().logger(), "fence {} destroyed", fmt::ptr(_handle));
    }

    auto fence_t::make(const device_t& device, bool signaled) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto fence = ir::arc_ptr<self>(new self());
        auto fence_info = VkFenceCreateInfo();
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.pNext = nullptr;
        fence_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0_u32;
        IR_VULKAN_CHECK(device.logger(), vkCreateFence(device.handle(), &fence_info, nullptr, &fence->_handle));
        IR_LOG_INFO(device.logger(), "fence {} created", fmt::ptr(fence->_handle));

        fence->_device = device.as_intrusive_ptr();
        return fence;
    }

    auto fence_t::make(const device_t& device, uint32 count, bool signaled) noexcept -> std::vector<arc_ptr<self>> {
        IR_PROFILE_SCOPED();
        auto fences = std::vector<arc_ptr<self>>(count);
        for (auto& fence : fences) {
            fence = make(device, signaled);
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
        return vkGetFenceStatus(_device.as_const_ref().handle(), _handle) == VK_SUCCESS;
    }

    auto fence_t::wait(uint64 timeout) const noexcept -> void {
        IR_PROFILE_SCOPED();
        IR_VULKAN_CHECK(_device.as_const_ref().logger(), vkWaitForFences(_device.as_const_ref().handle(), 1, &_handle, true, timeout));
    }

    auto fence_t::reset() const noexcept -> void {
        IR_PROFILE_SCOPED();
        IR_VULKAN_CHECK(_device.as_const_ref().logger(), vkResetFences(_device.as_const_ref().handle(), 1, &_handle));
    }
}
