#include <iris/core/hash.hpp>

#include <iris/gfx/device.hpp>
#include <iris/gfx/pipeline.hpp>
#include <iris/gfx/descriptor_set.hpp>
#include <iris/gfx/descriptor_pool.hpp>
#include <iris/gfx/descriptor_layout.hpp>

namespace ir {
    descriptor_set_t::descriptor_set_t(device_t& device) noexcept
        : _device(device) {
        IR_PROFILE_SCOPED();
    }

    descriptor_set_t::~descriptor_set_t() noexcept {
        IR_PROFILE_SCOPED();
        vkFreeDescriptorSets(device().handle(), pool().handle(), 1, &_handle);
        IR_LOG_INFO(device().logger(), "descriptor set {} freed", fmt::ptr(_handle));
    }

    auto descriptor_set_t::make(
        device_t& device,
        const descriptor_layout_t& layout
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto set = arc_ptr<self>(new self(device));
        const auto* pool = &device.descriptor_pool();
        const auto layout_handle = layout.handle();
        auto allocate_info = VkDescriptorSetAllocateInfo();
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.pNext = nullptr;
        allocate_info.descriptorPool = pool->handle();
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &layout_handle;
        auto result = vkAllocateDescriptorSets(device.handle(), &allocate_info, &set->_handle);

        if (result == VK_ERROR_OUT_OF_POOL_MEMORY) {
            IR_LOG_WARN(device.logger(), "descriptor_pool_t: memory exhausted, reallocating");
            auto new_sizes = akl::fast_hash_map<descriptor_type_t, uint32>();
            for (const auto& [type, size] : pool->sizes()) {
                for (const auto& binding : layout.bindings()) {
                    auto new_size = size;
                    if (type == binding.type) {
                        new_size = std::max(size * 2, size + binding.count);
                    }
                    new_sizes[type] = new_size;
                }
            }
            device.resize_descriptor_pool(new_sizes);
            pool = &device.descriptor_pool();
            allocate_info.descriptorPool = pool->handle();
            // if this fails we are in big trouble
            IR_VULKAN_CHECK(device.logger(), vkAllocateDescriptorSets(device.handle(), &allocate_info, &set->_handle));
        }
        IR_LOG_INFO(device.logger(), "allocated descriptor set {}", fmt::ptr(set->_handle));

        set->_pool = pool->as_intrusive_ptr();
        set->_layout = layout.as_intrusive_ptr();
        return set;
    }

    auto descriptor_set_t::handle() const noexcept -> VkDescriptorSet {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto descriptor_set_t::device() const noexcept -> device_t& {
        IR_PROFILE_SCOPED();
        return _device.get();
    }

    auto descriptor_set_t::pool() const noexcept -> const descriptor_pool_t& {
        IR_PROFILE_SCOPED();
        return *_pool;
    }

    auto descriptor_set_t::layout() const noexcept -> const descriptor_layout_t& {
        IR_PROFILE_SCOPED();
        return *_layout;
    }

    descriptor_set_builder_t::descriptor_set_builder_t(pipeline_t& pipeline, uint32 set) noexcept
        : _layout(std::cref(pipeline.descriptor_layout(set))) {
        IR_PROFILE_SCOPED();
        _binding.pool = pipeline.device().descriptor_pool().handle();
        _binding.layout = pipeline.descriptor_layout(set).handle();
        _binding.bindings.reserve(pipeline.descriptor_layout(set).bindings().size());
    }

    auto descriptor_set_builder_t::bind_uniform_buffer(uint32 binding, const buffer_info_t& buffer) noexcept -> self& {
        IR_PROFILE_SCOPED();
        _binding.bindings.emplace_back(descriptor_content_t {
            .binding = binding,
            .type = descriptor_type_t::e_uniform_buffer,
            .contents = { buffer }
        });
        return *this;
    }

    auto descriptor_set_builder_t::build() const noexcept -> arc_ptr<descriptor_set_t> {
        IR_PROFILE_SCOPED();
        auto& device = _layout.get().device();
        if (auto set = device.acquire_descriptor_set(_binding)) {
            return set;
        }
        auto set = device.register_descriptor_set(_binding, descriptor_set_t::make(_layout.get().device(), _layout.get()));
        auto buffer_infos = std::vector<std::vector<VkDescriptorBufferInfo>>();
        auto image_infos = std::vector<std::vector<VkDescriptorImageInfo>>();
        auto writes = std::vector<VkWriteDescriptorSet>();
        writes.reserve(_binding.bindings.size());
        for (const auto& binding : _binding.bindings) {
            auto write = VkWriteDescriptorSet();
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext = nullptr;
            write.dstSet = set.as_const_ref().handle();
            write.dstBinding = binding.binding;
            write.dstArrayElement = 0;
            write.descriptorCount = binding.contents.size();
            write.descriptorType = as_enum_counterpart(binding.type);

            switch (binding.type) {
                case descriptor_type_t::e_sampler:
                case descriptor_type_t::e_combined_image_sampler:
                case descriptor_type_t::e_sampled_image:
                case descriptor_type_t::e_storage_image:
                case descriptor_type_t::e_input_attachment:
                    image_infos.emplace_back().reserve(binding.contents.size());
                    for (const auto& content : binding.contents) {
                        const auto& image = std::get<image_info_t>(content);
                        image_infos.back().emplace_back(VkDescriptorImageInfo {
                            .sampler = image.sampler,
                            .imageView = image.view,
                            .imageLayout = as_enum_counterpart(image.layout)
                        });
                    }
                    write.pImageInfo = image_infos.back().data();
                    break;

                case descriptor_type_t::e_uniform_buffer:
                case descriptor_type_t::e_storage_buffer:
                case descriptor_type_t::e_uniform_buffer_dynamic:
                case descriptor_type_t::e_storage_buffer_dynamic:
                    buffer_infos.emplace_back().reserve(binding.contents.size());
                    for (const auto& content : binding.contents) {
                        const auto& buffer = std::get<buffer_info_t>(content);
                        buffer_infos.back().emplace_back(VkDescriptorBufferInfo {
                            .buffer = buffer.handle,
                            .offset = buffer.offset,
                            .range = buffer.size
                        });
                    }
                    write.pBufferInfo = buffer_infos.back().data();
                    break;

                case descriptor_type_t::e_uniform_texel_buffer:
                case descriptor_type_t::e_storage_texel_buffer:
                    // TODO
                    IR_ASSERT(false, "not implemented");
                    break;
            }
            writes.emplace_back(write);
        }
        vkUpdateDescriptorSets(device.handle(), writes.size(), writes.data(), 0, nullptr);
        return set;
    }
}
