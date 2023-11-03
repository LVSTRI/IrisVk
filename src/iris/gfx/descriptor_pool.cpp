#include <iris/gfx/device.hpp>
#include <iris/gfx/descriptor_pool.hpp>

#include <algorithm>
#include <numeric>

namespace ir {
    descriptor_pool_t::descriptor_pool_t(device_t& device) noexcept
        : _device(std::ref(device)) {
        IR_PROFILE_SCOPED();
    }

    descriptor_pool_t::~descriptor_pool_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroyDescriptorPool(_device.get().handle(), _handle, nullptr);
        IR_LOG_INFO(_device.get().logger(), "descriptor pool {} destroyed", fmt::ptr(_handle));
    }

    auto descriptor_pool_t::make(
        device_t& device,
        uint32 initial_capacity,
        const std::string& name
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        return make(device, { {
            std::make_pair(descriptor_type_t::e_sampler, initial_capacity),
            std::make_pair(descriptor_type_t::e_combined_image_sampler, initial_capacity * 16),
            std::make_pair(descriptor_type_t::e_sampled_image, initial_capacity),
            std::make_pair(descriptor_type_t::e_storage_image, initial_capacity),
            std::make_pair(descriptor_type_t::e_uniform_texel_buffer, initial_capacity),
            std::make_pair(descriptor_type_t::e_storage_texel_buffer, initial_capacity),
            std::make_pair(descriptor_type_t::e_uniform_buffer, initial_capacity),
            std::make_pair(descriptor_type_t::e_storage_buffer, initial_capacity),
            std::make_pair(descriptor_type_t::e_uniform_buffer_dynamic, initial_capacity),
            std::make_pair(descriptor_type_t::e_storage_buffer_dynamic, initial_capacity),
            std::make_pair(descriptor_type_t::e_input_attachment, initial_capacity),
        } }, name);
    }

    auto descriptor_pool_t::make(
        device_t& device,
        const descriptor_size_table& size,
        const std::string& name
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto pool = arc_ptr<self>(new self(device));
        auto pool_sizes = std::vector<VkDescriptorPoolSize>();
        pool_sizes.reserve(16);
        for (const auto& [type, count] : size) {
            pool_sizes.push_back({ as_enum_counterpart(type), count });
        }

        auto pool_create_info = VkDescriptorPoolCreateInfo();
        pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_create_info.pNext = nullptr;
        pool_create_info.flags =
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
            VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        pool_create_info.maxSets = std::accumulate(pool_sizes.begin(), pool_sizes.end(), 0_u32, [](auto x, const auto& each) {
            return x + each.descriptorCount;
        });
        pool_create_info.poolSizeCount = pool_sizes.size();
        pool_create_info.pPoolSizes = pool_sizes.data();
        IR_VULKAN_CHECK(device.logger(), vkCreateDescriptorPool(device.handle(), &pool_create_info, nullptr, &pool->_handle));
        IR_LOG_INFO(device.logger(), "descriptor pool initialized, current capacity: {}", pool_create_info.maxSets);
        pool->_sizes = std::move(size);

        if (!name.empty()) {
            device.set_debug_name({
                .type = VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                .handle = reinterpret_cast<uint64>(pool->_handle),
                .name = name.c_str()
            });
        }
        return pool;
    }

    auto descriptor_pool_t::handle() const noexcept -> VkDescriptorPool {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto descriptor_pool_t::device() const noexcept -> device_t& {
        IR_PROFILE_SCOPED();
        return _device.get();
    }

    auto descriptor_pool_t::sizes() const noexcept -> std::span<const std::pair<descriptor_type_t, uint32>> {
        IR_PROFILE_SCOPED();
        return _sizes.values();
    }

    auto descriptor_pool_t::capacity(descriptor_type_t type) const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _sizes.at(type);
    }
}
