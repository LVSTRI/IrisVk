#include <iris/gfx/device.hpp>
#include <iris/gfx/semaphore.hpp>

namespace ir {
    semaphore_t::semaphore_t() noexcept = default;

    semaphore_t::~semaphore_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroySemaphore(device().handle(), _handle, nullptr);
        IR_LOG_INFO(device().logger(), "semaphore {} destroyed", fmt::ptr(_handle));
    }

    auto semaphore_t::make(const device_t& device) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto semaphore = ir::arc_ptr<self>(new self());
        auto semaphore_info = VkSemaphoreCreateInfo();
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_info.pNext = nullptr;
        semaphore_info.flags = {};
        IR_VULKAN_CHECK(device.logger(), vkCreateSemaphore(device.handle(), &semaphore_info, nullptr, &semaphore->_handle));
        IR_LOG_INFO(device.logger(), "semaphore {} created", fmt::ptr(semaphore->_handle));

        semaphore->_device = device.as_intrusive_ptr();
        return semaphore;
    }

    auto semaphore_t::make(const device_t& device, uint32 count) noexcept -> std::vector<arc_ptr<self>> {
        IR_PROFILE_SCOPED();
        auto semaphores = std::vector<arc_ptr<self>>(count);
        for (auto& semaphore : semaphores) {
            semaphore = make(device);
        }
        return semaphores;
    }

    auto semaphore_t::handle() const noexcept -> VkSemaphore {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto semaphore_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }
}
