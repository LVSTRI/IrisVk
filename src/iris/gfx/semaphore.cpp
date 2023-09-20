#include <iris/gfx/device.hpp>
#include <iris/gfx/semaphore.hpp>

namespace ir {
    semaphore_t::semaphore_t() noexcept = default;

    semaphore_t::~semaphore_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroySemaphore(device().handle(), _handle, nullptr);
        IR_LOG_INFO(device().logger(), "semaphore {} destroyed", fmt::ptr(_handle));
    }

    auto semaphore_t::make(const device_t& device, const semaphore_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto semaphore = arc_ptr<self>(new self());
        auto timeline_semaphore_info = VkSemaphoreTypeCreateInfo();
        timeline_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timeline_semaphore_info.pNext = nullptr;
        timeline_semaphore_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timeline_semaphore_info.initialValue = info.counter;

        auto semaphore_info = VkSemaphoreCreateInfo();
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (info.timeline) {
            semaphore_info.pNext = &timeline_semaphore_info;
        }
        semaphore_info.flags = {};
        IR_VULKAN_CHECK(device.logger(), vkCreateSemaphore(device.handle(), &semaphore_info, nullptr, &semaphore->_handle));
        IR_LOG_INFO(device.logger(), "semaphore {} created", fmt::ptr(semaphore->_handle));

        semaphore->_counter = info.counter;
        semaphore->_is_timeline = info.timeline;
        semaphore->_device = device.as_intrusive_ptr();
        return semaphore;
    }

    auto semaphore_t::make(const device_t& device, uint32 count, const semaphore_create_info_t& info) noexcept -> std::vector<arc_ptr<self>> {
        IR_PROFILE_SCOPED();
        auto semaphores = std::vector<arc_ptr<self>>(count);
        for (auto& semaphore : semaphores) {
           semaphore = make(device, info);
        }
        return semaphores;
    }

    auto semaphore_t::handle() const noexcept -> VkSemaphore {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto semaphore_t::counter() const noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        return _counter;
    }

    auto semaphore_t::is_timeline() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _is_timeline;
    }

    auto semaphore_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }

    auto semaphore_t::increment(uint64 x) noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        const auto old = _counter;
        _counter += x;
        return old;
    }
}
