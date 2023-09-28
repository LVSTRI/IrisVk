#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <iris/gfx/device.hpp>
#include <iris/gfx/queue.hpp>
#include <iris/gfx/fence.hpp>

#include <volk.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <vector>
#include <array>
#include <span>

namespace ir {
    enum class buffer_flag_t {
        e_shared = 1 << 0,
        e_mapped = 1 << 1,
        e_random_access = 1 << 2,
        e_resized = 1 << 3,
    };

    struct memory_properties_t {
        constexpr auto operator ==(const memory_properties_t& other) const noexcept -> bool = default;

        memory_property_t required = {};
        memory_property_t preferred = {};
    };
    constexpr static auto infer_memory_properties = memory_properties_t();

    struct buffer_create_info_t {
        buffer_usage_t usage = {};
        memory_properties_t memory = infer_memory_properties;
        buffer_flag_t flags = {};
        uint64 capacity = 0;
    };

    template <typename T>
    class buffer_t : public enable_intrusive_refcount_t<buffer_t<T>> {
    public:
        using self = buffer_t;

        buffer_t() noexcept;
        ~buffer_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device, const buffer_create_info_t& info) noexcept -> arc_ptr<self>;
        IR_NODISCARD static auto make(const device_t& device, uint32 count, const buffer_create_info_t& info) noexcept -> std::vector<arc_ptr<self>>;

        IR_NODISCARD auto handle() const noexcept -> VkBuffer;
        IR_NODISCARD auto memory() const noexcept -> VkDeviceMemory;
        IR_NODISCARD auto allocation() const noexcept -> VmaAllocation;
        IR_NODISCARD auto allocation_info() const noexcept -> const VmaAllocationInfo&;

        IR_NODISCARD auto buffer_usage() const noexcept -> buffer_usage_t;
        IR_NODISCARD auto memory_usage() const noexcept -> memory_properties_t;

        IR_NODISCARD auto alignment() const noexcept -> uint64;
        IR_NODISCARD auto capacity() const noexcept -> uint64;
        IR_NODISCARD auto size() const noexcept -> uint64;
        IR_NODISCARD auto address() const noexcept -> uint64;

        IR_NODISCARD auto data() noexcept -> T*;
        IR_NODISCARD auto data() const noexcept -> const T*;

        IR_NODISCARD auto slice(uint64 offset = 0, uint64 size = whole_size) const noexcept -> buffer_info_t;
        IR_NODISCARD auto info() const noexcept -> const buffer_create_info_t&;
        IR_NODISCARD auto device() const noexcept -> const device_t&;

        IR_NODISCARD auto is_empty() const noexcept -> bool;
        IR_NODISCARD auto is_shared() const noexcept -> bool;
        IR_NODISCARD auto is_seq_write_only() const noexcept -> bool;

        IR_NODISCARD auto operator [](uint64 index) noexcept -> T&;
        IR_NODISCARD auto operator [](uint64 index) const noexcept -> const T&;

        IR_NODISCARD auto size_bytes() const noexcept -> uint64;

        auto insert(uint64 offset, uint64 size, const void* ptr) noexcept -> void;
        auto insert(const T& value) noexcept -> void;
        auto insert(uint64 offset, const T& value) noexcept -> void;
        auto insert(std::span<const T> values) noexcept -> void;
        auto insert(uint64 offset, std::span<const T> values) noexcept -> void;
        template <uint64 N>
        auto insert(const T(&values)[N]) noexcept -> void;
        template <uint64 N>
        auto insert(uint64 offset, const T(&values)[N]) noexcept -> void;
        auto fill(const uint64& value, uint64 size = -1) noexcept -> void;

        auto push_back(const T& value) noexcept -> void;
        auto pop_back() noexcept -> void;

        // potentially destructive
        auto resize(uint64 size) noexcept -> void;
        // destructive
        auto reserve(uint64 capacity) noexcept -> void;
        auto clear() noexcept -> void;

    private:
        static auto _make(const device_t& device, const buffer_create_info_t& info, self* buffer) noexcept -> void;

        VkBuffer _handle = {};
        VmaAllocation _allocation = {};
        VmaAllocationInfo _allocation_info = {};

        uint64 _alignment = 0;
        uint64 _capacity = 0;
        uint64 _size = 0;
        uint64 _address = 0;

        void* _data = nullptr;

        buffer_create_info_t _info = {};
        arc_ptr<const device_t> _device;
    };

    template <typename T>
    auto upload_buffer(device_t& device, std::span<const T> data, const buffer_create_info_t& info) noexcept -> arc_ptr<buffer_t<T>> {
        IR_PROFILE_SCOPED();
        auto staging = buffer_t<T>::make(device, {
            .usage = buffer_usage_t::e_transfer_src,
            .flags = buffer_flag_t::e_mapped,
            .capacity = data.size(),
        });
        auto upload = buffer_t<T>::make(device, {
            .usage = buffer_usage_t::e_transfer_dst | info.usage,
            .memory = info.memory,
            .flags = info.flags | buffer_flag_t::e_resized,
            .capacity = data.size(),
        });
        staging->insert(data);
        const auto& pool = device.transfer_queue().transient_pool(0);
        auto command_buffer = command_buffer_t::make(pool, {});
        command_buffer->begin();
        command_buffer->copy_buffer(staging->slice(), upload->slice(), {});
        command_buffer->end();
        auto fence = fence_t::make(device, false);
        device.transfer_queue().submit({
            .command_buffers = { std::cref(*command_buffer) }
        }, fence.get());
        fence->wait();
        return upload;
    }

    template <typename T>
    buffer_t<T>::buffer_t() noexcept = default;

    template <typename T>
    buffer_t<T>::~buffer_t() noexcept {
        IR_PROFILE_SCOPED();
        IR_LOG_INFO(device().logger(), "destroying buffer {}", fmt::ptr(_handle));
        vmaDestroyBuffer(device().allocator(), _handle, _allocation);
    }

    template <typename T>
    auto buffer_t<T>::make(const device_t& device, const buffer_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto buffer = arc_ptr<self>(new self());
        _make(device, info, buffer.get());
        return buffer;
    }

    template <typename T>
    auto buffer_t<T>::make(const device_t& device, uint32 count, const buffer_create_info_t& info) noexcept -> std::vector<arc_ptr<self>> {
        IR_PROFILE_SCOPED();
        auto buffers = std::vector<arc_ptr<self>>(count);
        for (auto& buffer : buffers) {
            buffer = make(device, info);
        }
        return buffers;
    }

    template <typename T>
    auto buffer_t<T>::handle() const noexcept -> VkBuffer {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    template <typename T>
    auto buffer_t<T>::memory() const noexcept -> VkDeviceMemory {
        IR_PROFILE_SCOPED();
        return _allocation_info.deviceMemory;
    }

    template <typename T>
    auto buffer_t<T>::allocation() const noexcept -> VmaAllocation {
        IR_PROFILE_SCOPED();
        return _allocation;
    }

    template <typename T>
    auto buffer_t<T>::allocation_info() const noexcept -> const VmaAllocationInfo& {
        IR_PROFILE_SCOPED();
        return _allocation_info;
    }

    template <typename T>
    auto buffer_t<T>::buffer_usage() const noexcept -> buffer_usage_t {
        IR_PROFILE_SCOPED();
        return _info.usage;
    }

    template <typename T>
    auto buffer_t<T>::memory_usage() const noexcept -> memory_properties_t {
        IR_PROFILE_SCOPED();
        return _info.memory;
    }

    template <typename T>
    auto buffer_t<T>::alignment() const noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        return _alignment;
    }

    template <typename T>
    auto buffer_t<T>::capacity() const noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        return _capacity;
    }

    template <typename T>
    auto buffer_t<T>::size() const noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        return _size;
    }

    template <typename T>
    auto buffer_t<T>::address() const noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        return _address;
    }

    template <typename T>
    auto buffer_t<T>::data() noexcept -> T* {
        IR_PROFILE_SCOPED();
        return static_cast<T*>(_data);
    }

    template <typename T>
    auto buffer_t<T>::data() const noexcept -> const T* {
        IR_PROFILE_SCOPED();
        return static_cast<const T*>(_data);
    }

    template <typename T>
    auto buffer_t<T>::slice(uint64 offset, uint64 size) const noexcept -> buffer_info_t {
        IR_PROFILE_SCOPED();
        return buffer_info_t {
            _handle,
            offset * sizeof(T),
            size == whole_size ?
                size_bytes() :
                size * sizeof(T),
            _address
        };
    }

    template <typename T>
    auto buffer_t<T>::info() const noexcept -> const buffer_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    template <typename T>
    auto buffer_t<T>::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }

    template <typename T>
    auto buffer_t<T>::is_empty() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return _size == 0;
    }

    template <typename T>
    auto buffer_t<T>::is_shared() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return (_info.flags & buffer_flag_t::e_shared) == buffer_flag_t::e_shared;
    }

    template <typename T>
    auto buffer_t<T>::is_seq_write_only() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return (_info.flags & buffer_flag_t::e_random_access) != buffer_flag_t::e_random_access;
    }

    template <typename T>
    auto buffer_t<T>::operator [](uint64 index) noexcept -> T& {
        IR_PROFILE_SCOPED();
        return data()[index];
    }

    template <typename T>
    auto buffer_t<T>::operator [](uint64 index) const noexcept -> const T& {
        IR_PROFILE_SCOPED();
        return data()[index];
    }

    template <typename T>
    auto buffer_t<T>::size_bytes() const noexcept -> uint64 {
        IR_PROFILE_SCOPED();
        return _size * sizeof(T);
    }

    template <typename T>
    auto buffer_t<T>::insert(uint64 offset, uint64 bytes, const void* ptr) noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto size = bytes / sizeof(T);
        const auto where = offset / sizeof(T);
        if (size + where > _capacity) {
            reserve(std::max(_capacity * 2, size + where + 1));
        }
        std::memcpy(data() + where, ptr, bytes);
        _size = std::max(_size, size + where);
    }

    template <typename T>
    auto buffer_t<T>::insert(const T& value) noexcept -> void {
        IR_PROFILE_SCOPED();
        insert(0, value);
    }

    template <typename T>
    auto buffer_t<T>::insert(uint64 offset, const T& value) noexcept -> void {
        IR_PROFILE_SCOPED();
        insert(offset * sizeof(T), { &value, 1 });
    }

    template <typename T>
    auto buffer_t<T>::insert(std::span<const T> values) noexcept -> void {
        IR_PROFILE_SCOPED();
        insert(0, values);
    }

    template <typename T>
    auto buffer_t<T>::insert(uint64 offset, std::span<const T> values) noexcept -> void {
        IR_PROFILE_SCOPED();
        insert(offset * sizeof(T), values.size_bytes(), values.data());
    }

    template <typename T>
    template <uint64 N>
    auto buffer_t<T>::insert(const T(&values)[N]) noexcept -> void {
        IR_PROFILE_SCOPED();
        insert(0, values);
    }

    template <typename T>
    template <uint64 N>
    auto buffer_t<T>::insert(uint64 offset, const T(&values)[N]) noexcept -> void {
        IR_PROFILE_SCOPED();
        insert(offset * sizeof(T), std::span<const T>(values));
    }

    template <typename T>
    auto buffer_t<T>::fill(const uint64& value, uint64 size) noexcept -> void {
        IR_PROFILE_SCOPED();
        std::memset(data(), value, std::min(size, _capacity * sizeof(T)));
        _size = _capacity;
    }

    template <typename T>
    auto buffer_t<T>::push_back(const T& value) noexcept -> void {
        IR_PROFILE_SCOPED();
        if (_size == _capacity) {
            reserve(_capacity * 2);
        }
        std::memcpy(data() + _size, &value, sizeof(T));
        ++_size;
    }

    template <typename T>
    auto buffer_t<T>::pop_back() noexcept -> void {
        IR_PROFILE_SCOPED();
        --_size;
    }

    template <typename T>
    auto buffer_t<T>::resize(uint64 size) noexcept -> void {
        IR_PROFILE_SCOPED();
        if (size > _capacity) {
            reserve(std::max(size, _capacity * 2));
        }
        _size = size;
    }

    template <typename T>
    auto buffer_t<T>::reserve(uint64 capacity) noexcept -> void {
        IR_PROFILE_SCOPED();
        if (_capacity <= capacity) {
            return;
        }
        IR_LOG_WARN(device().logger(), "growing buffer capacity {} -> {}", _capacity, capacity);
        auto old_handle = _handle;
        auto old_allocation = _allocation;
        _make(device(), info(), this);
        vmaDestroyBuffer(device().allocator(), old_handle, old_allocation);
    }

    template <typename T>
    auto buffer_t<T>::clear() noexcept -> void {
        IR_PROFILE_SCOPED();
        _size = 0;
    }

    template <typename T>
    auto buffer_t<T>::_make(const device_t& device, const buffer_create_info_t& info, self* buffer) noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto is_bda_supported = device.is_supported(device_feature_t::e_buffer_device_address);
        const auto is_shared = (info.flags & buffer_flag_t::e_shared) == buffer_flag_t::e_shared;
        const auto is_mapped = (info.flags & buffer_flag_t::e_mapped) == buffer_flag_t::e_mapped;
        const auto is_random_access = (info.flags & buffer_flag_t::e_random_access) == buffer_flag_t::e_random_access;
        const auto is_resized = (info.flags & buffer_flag_t::e_resized) == buffer_flag_t::e_resized;
        auto buffer_usage = info.usage;
        if (is_bda_supported) {
            buffer_usage |= buffer_usage_t::e_shader_device_address;
        }
        auto queue_families = std::to_array({
            device.graphics_queue().family(),
            device.compute_queue().family(),
            device.transfer_queue().family(),
        });
        auto queue_family_count = 1_u32;
        if (queue_families[0] != queue_families[1]) {
            queue_family_count++;
        } else if (queue_families[0] != queue_families[2]) {
            queue_family_count++;
            std::swap(queue_families[1], queue_families[2]);
        }
        if (queue_families[0] != queue_families[2] &&
            queue_families[1] != queue_families[2] &&
            queue_families[0] != queue_families[1]) {
            queue_family_count++;
        }

        auto buffer_info = VkBufferCreateInfo();
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.pNext = nullptr;
        buffer_info.flags = {};
        buffer_info.size = info.capacity * sizeof(T);
        buffer_info.usage = as_enum_counterpart(buffer_usage);
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (is_shared && queue_family_count >= 2) {
            buffer_info.sharingMode = VK_SHARING_MODE_CONCURRENT;
            buffer_info.queueFamilyIndexCount = queue_family_count;
            buffer_info.pQueueFamilyIndices = queue_families.data();
        }

        auto memory_usage = info.memory;
        auto allocation_extra_info = VmaAllocationInfo();
        auto allocation_info = VmaAllocationCreateInfo();
        if (is_mapped) {
            allocation_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
            memory_usage.required |= memory_property_t::e_host_visible | memory_property_t::e_host_coherent;
            if (is_random_access) {
                allocation_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
                memory_usage.required |= memory_property_t::e_host_cached;
            } else {
                allocation_info.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }
        }
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
        allocation_info.requiredFlags = as_enum_counterpart(memory_usage.required);
        allocation_info.preferredFlags = as_enum_counterpart(memory_usage.preferred);
        allocation_info.memoryTypeBits = {};
        allocation_info.pool = {};
        allocation_info.pUserData = nullptr;
        allocation_info.priority = 1.0f;
        IR_VULKAN_CHECK(
            device.logger(),
            vmaCreateBuffer(
                device.allocator(),
                &buffer_info,
                &allocation_info,
                &buffer->_handle,
                &buffer->_allocation,
                &allocation_extra_info));
        IR_LOG_INFO(device.logger(), "allocated buffer {}, (size: {}, usage: {})",
            fmt::ptr(buffer->_handle),
            info.capacity,
            as_string(buffer_usage));
        auto memory_requirements = VkMemoryRequirements();
        vkGetBufferMemoryRequirements(device.handle(), buffer->_handle, &memory_requirements);
        auto memory_property = VkMemoryPropertyFlags();
        vmaGetMemoryTypeProperties(device.allocator(), allocation_extra_info.memoryType, &memory_property);
        buffer->_allocation_info = allocation_extra_info;
        buffer->_alignment = memory_requirements.alignment;
        buffer->_capacity = info.capacity;
        buffer->_size = 0;
        if (is_resized) {
            buffer->_size = info.capacity;
        }
        if (is_bda_supported) {
            auto bda_info = VkBufferDeviceAddressInfo();
            bda_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bda_info.pNext = nullptr;
            bda_info.buffer = buffer->_handle;
            buffer->_address = vkGetBufferDeviceAddress(device.handle(), &bda_info);
        }
        if (is_mapped) {
            buffer->_data = allocation_extra_info.pMappedData;
        }
        buffer->_info = info;
        buffer->_device = device.as_intrusive_ptr();
    }
}
