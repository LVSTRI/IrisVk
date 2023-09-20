#include <iris/gfx/device.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/sparse_page_allocator.hpp>

namespace ir {
    sparse_page_allocator_t::sparse_page_allocator_t(const image_t& image) noexcept
        : _image(std::cref(image)) {
        IR_PROFILE_SCOPED();
    }

    sparse_page_allocator_t::~sparse_page_allocator_t() noexcept {
        IR_PROFILE_SCOPED();
    }

    auto sparse_page_allocator_t::make(const device_t& device, const image_t& image) noexcept -> self {
        IR_PROFILE_SCOPED();
        auto allocator = self(image);
        IR_ASSERT(image.is_sparsely_resident(), "image is not sparsely resident");
        const auto granularity = image.granularity();
        auto memory_requirements = VkMemoryRequirements2();
        memory_requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        auto page_memory_info = VkImageCreateInfo();
        page_memory_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        page_memory_info.pNext = nullptr;
        page_memory_info.flags = 0;
        page_memory_info.imageType = VK_IMAGE_TYPE_2D;
        page_memory_info.format = VK_FORMAT_R32_UINT;
        page_memory_info.extent = VkExtent3D { granularity.width, granularity.height, 1 };
        page_memory_info.mipLevels = 1;
        page_memory_info.arrayLayers = 1;
        page_memory_info.samples = VK_SAMPLE_COUNT_1_BIT;
        page_memory_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        page_memory_info.usage = as_enum_counterpart(image.usage());
        page_memory_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        page_memory_info.queueFamilyIndexCount = 0;
        page_memory_info.pQueueFamilyIndices = nullptr;
        page_memory_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkGetDeviceImageMemoryRequirements(device.handle(), as_const_ptr(VkDeviceImageMemoryRequirements {
            .sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS,
            .pNext = nullptr,
            .pCreateInfo = &page_memory_info,
            .planeAspect = as_enum_counterpart(image.view().aspect()),
        }), &memory_requirements);
        allocator._pages = std::vector<sparse_memory_page_t>(granularity.width * granularity.height);
        allocator._page_info = memory_requirements.memoryRequirements;
        allocator._device = device.as_intrusive_ptr();
        return allocator;
    }

    auto sparse_page_allocator_t::request_pages(std::span<const uint8> req) noexcept -> std::vector<sparse_image_memory_opaque_bind_t> {
        IR_PROFILE_SCOPED();
        auto bindings = std::vector<sparse_image_memory_opaque_bind_t>();
        bindings.reserve(req.size());
        if (req.empty()) {
            return bindings;
        }
        if (_blocks.empty()) {
            auto memory_info = VkMemoryAllocateInfo();
            memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memory_info.pNext = nullptr;
            memory_info.allocationSize = sparse_memory_block_t::byte_size;
            memory_info.memoryTypeIndex = _device->memory_type_index(_page_info.memoryTypeBits, memory_property_t::e_device_local);
            auto memory = VkDeviceMemory();
            IR_VULKAN_CHECK(_device->logger(), vkAllocateMemory(_device->handle(), &memory_info, nullptr, &memory));
            _blocks.emplace_back(sparse_memory_block_t {
                .pages = {},
                .memory = memory,
                .allocations = 0,
            });
        }
        for (auto page = 0_u32; page < req.size(); ++page) {
            if (req[page] != 0) {
                if (_pages[page].block) {
                    continue;
                }
                for (auto i = 0_u32; i < _blocks.size(); ++i) {
                    auto& block = _blocks[i];
                    if (block.allocations >= sparse_memory_block_t::max_allocations) {
                        continue;
                    }
                    // find first free page
                    for (auto j = 0_u32; j < sparse_memory_block_t::page_table_size; ++j) {
                        auto& mask = block.pages[j];
                        if (mask == ~0_u64) {
                            continue;
                        }
                        const auto index = std::countl_zero(~mask);
                        mask |= 1_u64 << (63 - index);
                        _pages[page] = sparse_memory_page_t {
                            .block = &_blocks[i],
                            .offset = (j * 64 + index) * sparse_memory_block_t::page_byte_size,
                            .size = sparse_memory_block_t::page_byte_size,
                        };
                        bindings.emplace_back(sparse_image_memory_opaque_bind_t {
                            .image = &_image.get(),
                            .page = &_pages[page],
                            .offset = page * _page_info.size,
                        });
                        ++block.allocations;
                        break;
                    }
                }
            } else {
                if (_pages[page].block) {
                    // free page
                    auto& block = *_pages[page].block;
                    const auto index = _pages[page].offset / sparse_memory_block_t::page_byte_size;
                    const auto offset = index / 64;
                    const auto bit = index % 64;
                    block.pages[offset] &= ~(1_u64 << (63 - bit));
                    --block.allocations;
                    _pages[page] = {};
                }
            }
        }
        return bindings;
    }

    auto sparse_page_allocator_t::block(uint64 index) const noexcept -> const sparse_memory_block_t& {
        IR_PROFILE_SCOPED();
        return _blocks[index];
    }

    auto sparse_page_allocator_t::page(uint64 index) const noexcept -> const sparse_memory_page_t& {
        IR_PROFILE_SCOPED();
        return _pages[index];
    }
}

