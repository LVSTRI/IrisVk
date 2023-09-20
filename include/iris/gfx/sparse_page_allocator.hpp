#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <memory>
#include <span>

namespace ir {
    struct sparse_memory_block_t {
        constexpr static auto byte_size = 256_MiB;
        constexpr static auto page_byte_size = 64_KiB;
        constexpr static auto block_size = 8192;
        constexpr static auto page_size = 128;
        constexpr static auto page_table_size = block_size / page_size;
        constexpr static auto max_allocations = 4096;

        std::array<uint64, page_table_size> pages;
        VkDeviceMemory memory = {};
        uint64 allocations = 0;
    };

    struct sparse_memory_page_t {
        sparse_memory_block_t* block = nullptr;
        uint64 offset = 0;
        uint64 size = 0;
    };

    struct sparse_image_memory_opaque_bind_t {
        const image_t* image = nullptr;
        sparse_memory_page_t* page = nullptr;
        uint64 offset = 0;
    };

    class sparse_page_allocator_t {
    public:
        using self = sparse_page_allocator_t;

        sparse_page_allocator_t(const image_t& image) noexcept;
        ~sparse_page_allocator_t() noexcept;

        IR_NODISCARD static auto make(const device_t& device, const image_t& image) noexcept -> self;

        IR_NODISCARD auto request_pages(std::span<const uint8> pages) noexcept -> std::vector<sparse_image_memory_opaque_bind_t>;

        IR_NODISCARD auto block(uint64 index) const noexcept -> const sparse_memory_block_t&;
        IR_NODISCARD auto page(uint64 index) const noexcept -> const sparse_memory_page_t&;

    private:
        std::vector<sparse_memory_block_t> _blocks;
        std::vector<sparse_memory_page_t> _pages;
        VkMemoryRequirements _page_info = {};

        std::reference_wrapper<const image_t> _image;
        arc_ptr<const device_t> _device;
    };
}
