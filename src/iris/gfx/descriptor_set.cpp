#include <iris/core/hash.hpp>

#include <iris/gfx/device.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/pipeline.hpp>
#include <iris/gfx/descriptor_set.hpp>
#include <iris/gfx/texture.hpp>
#include <iris/gfx/descriptor_pool.hpp>
#include <iris/gfx/descriptor_layout.hpp>
#include <iris/gfx/sampler.hpp>

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
        auto dynamic_count = 0_u32;
        auto variable_count_info = VkDescriptorSetVariableDescriptorCountAllocateInfo();
        variable_count_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        variable_count_info.pNext = nullptr;

        auto allocate_info = VkDescriptorSetAllocateInfo();
        if (layout.is_dynamic()) {
            for (const auto& binding : layout.bindings()) {
                if (binding.dynamic) {
                    dynamic_count = binding.count;
                    break;
                }
            }
            variable_count_info.descriptorSetCount = 1;
            variable_count_info.pDescriptorCounts = &dynamic_count;
            allocate_info.pNext = &variable_count_info;
        }
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
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

    descriptor_set_builder_t::descriptor_set_builder_t(const descriptor_layout_t& layout) noexcept
        : _layout(std::cref(layout)) {
        IR_PROFILE_SCOPED();
        _binding.pool = layout.device().descriptor_pool().handle();
        _binding.layout = layout.handle();
        _binding.bindings.reserve(1024);
    }

    descriptor_set_builder_t::descriptor_set_builder_t(const pipeline_t& pipeline, uint32 set) noexcept
        : self(pipeline.descriptor_layout(set)) {
        IR_PROFILE_SCOPED();
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

    auto descriptor_set_builder_t::bind_storage_buffer(uint32 binding, const buffer_info_t& buffer) noexcept -> self& {
        IR_PROFILE_SCOPED();
        _binding.bindings.emplace_back(descriptor_content_t {
            .binding = binding,
            .type = descriptor_type_t::e_storage_buffer,
            .contents = { buffer }
        });
        return *this;
    }

    auto descriptor_set_builder_t::bind_storage_image(uint32 binding, const image_view_t& view) noexcept -> self& {
        IR_PROFILE_SCOPED();
        _binding.bindings.emplace_back(descriptor_content_t {
            .binding = binding,
            .type = descriptor_type_t::e_storage_image,
            .contents = {
                image_info_t {
                    .view = view.handle(),
                    .layout = image_layout_t::e_general
                }
            }
        });
        return *this;
    }

    auto descriptor_set_builder_t::bind_texture(uint32 binding, const texture_t& texture) noexcept -> self& {
        IR_PROFILE_SCOPED();
        _binding.bindings.emplace_back(descriptor_content_t {
            .binding = binding,
            .type = descriptor_type_t::e_combined_image_sampler,
            .contents = { texture.info() }
        });
        return *this;
    }

    auto descriptor_set_builder_t::bind_textures(uint32 binding, const std::vector<arc_ptr<texture_t>>& textures) noexcept -> self& {
        IR_PROFILE_SCOPED();
        auto infos = std::vector<descriptor_data>();
        infos.reserve(textures.size());
        for (const auto& texture : textures) {
            infos.emplace_back(texture->info());
        }
        _binding.bindings.emplace_back(descriptor_content_t {
            .binding = binding,
            .type = descriptor_type_t::e_combined_image_sampler,
            .contents = { std::move(infos) }
        });
        return *this;
    }

    auto descriptor_set_builder_t::bind_sampled_image(uint32 binding, const image_view_t& view, image_layout_t layout) noexcept -> self& {
        IR_PROFILE_SCOPED();
        _binding.bindings.emplace_back(descriptor_content_t {
            .binding = binding,
            .type = descriptor_type_t::e_sampled_image,
            .contents = {
                image_info_t {
                    .sampler = {},
                    .view = view.handle(),
                    .layout = layout,
                },
            }
        });
        return *this;
    }

    auto descriptor_set_builder_t::bind_combined_image_sampler(uint32 binding, const image_view_t& view, const sampler_t& sampler) noexcept -> self& {
        IR_PROFILE_SCOPED();
        _binding.bindings.emplace_back(descriptor_content_t {
            .binding = binding,
            .type = descriptor_type_t::e_combined_image_sampler,
            .contents = {
                image_info_t {
                    .sampler = sampler.handle(),
                    .view = view.handle(),
                    .layout = image_layout_t::e_shader_read_only_optimal,
                },
            }
        });
        return *this;
    }

    auto descriptor_set_builder_t::bind_combined_image_sampler(
        uint32 binding,
        std::span<std::reference_wrapper<const image_view_t>> views,
        const sampler_t& sampler
    ) noexcept -> self& {
        IR_PROFILE_SCOPED();
        auto infos = std::vector<descriptor_data>();
        infos.reserve(views.size());
        for (const auto& view : views) {
            infos.emplace_back(image_info_t {
                .sampler = sampler.handle(),
                .view = view.get().handle(),
                .layout = image_layout_t::e_shader_read_only_optimal,
            });
        }
        _binding.bindings.emplace_back(descriptor_content_t {
            .binding = binding,
            .type = descriptor_type_t::e_combined_image_sampler,
            .contents = { std::move(infos) }
        });

        return *this;
    }

    auto descriptor_set_builder_t::build() const noexcept -> arc_ptr<descriptor_set_t> {
        IR_PROFILE_SCOPED();
        auto& device = _layout.get().device();
        auto& cache = device.cache<descriptor_set_t>();
        if (cache.contains(_binding)) {
            return cache.acquire(_binding);
        }
        auto set = cache.insert(_binding, descriptor_set_t::make(device, _layout.get()));
        auto buffer_infos = std::vector<std::vector<VkDescriptorBufferInfo>>();
        auto image_infos = std::vector<std::vector<VkDescriptorImageInfo>>();
        auto writes = std::vector<VkWriteDescriptorSet>();
        writes.reserve(_binding.bindings.size());
        for (const auto& binding : _binding.bindings) {
            if (binding.contents.empty()) {
                continue;
            }
            auto write = VkWriteDescriptorSet();
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext = nullptr;
            write.dstSet = set->handle();
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

                default:
                    break;
            }
            writes.emplace_back(write);
        }
        IR_LOG_WARN(device.logger(), "descriptor_set_t ({}): cache miss", fmt::ptr(set->handle()));
        vkUpdateDescriptorSets(device.handle(), writes.size(), writes.data(), 0, nullptr);
        return set;
    }
}
