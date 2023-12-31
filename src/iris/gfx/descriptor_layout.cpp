#include <iris/core/hash.hpp>

#include <iris/gfx/device.hpp>
#include <iris/gfx/descriptor_layout.hpp>

namespace ir {
    descriptor_layout_t::descriptor_layout_t(device_t& device) noexcept
        : _device(std::ref(device)) {
        IR_PROFILE_SCOPED();
    }

    descriptor_layout_t::~descriptor_layout_t() noexcept {
        IR_PROFILE_SCOPED();
        vkDestroyDescriptorSetLayout(device().handle(), _handle, nullptr);
        IR_LOG_INFO(device().logger(), "descriptor layout {} freed", fmt::ptr(_handle));
    }

    auto descriptor_layout_t::make(device_t& device, const descriptor_layout_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto layout = arc_ptr<self>(new self(std::ref(device)));
        auto bindings_info = std::vector<VkDescriptorSetLayoutBinding>(info.bindings.size());
        for (const auto& binding : info.bindings) {
            bindings_info[binding.binding] = VkDescriptorSetLayoutBinding {
                .binding = binding.binding,
                .descriptorType = as_enum_counterpart(binding.type),
                .descriptorCount = binding.count,
                .stageFlags = static_cast<VkShaderStageFlags>(
                    as_enum_counterpart(binding.stage)),
                .pImmutableSamplers = nullptr
            };
        }

        auto binding_flags = std::vector<VkDescriptorBindingFlags>(info.bindings.size());
        binding_flags.reserve(info.bindings.size());
        for (const auto& binding : info.bindings) {
            binding_flags[binding.binding] = as_enum_counterpart(binding.flags);
        }

        auto binding_flags_info = VkDescriptorSetLayoutBindingFlagsCreateInfo();
        binding_flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        binding_flags_info.pNext = nullptr;
        binding_flags_info.bindingCount = binding_flags.size();
        binding_flags_info.pBindingFlags = binding_flags.data();

        auto descriptor_layout_info = VkDescriptorSetLayoutCreateInfo();
        descriptor_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_layout_info.pNext = &binding_flags_info;
        descriptor_layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        descriptor_layout_info.bindingCount = bindings_info.size();
        descriptor_layout_info.pBindings = bindings_info.data();
        IR_VULKAN_CHECK(
            device.logger(),
            vkCreateDescriptorSetLayout(
                device.handle(),
                &descriptor_layout_info,
                nullptr,
                &layout->_handle));
        IR_LOG_INFO(device.logger(), "descriptor layout initialized {}", fmt::ptr(layout->_handle));
        layout->_bindings = std::vector(info.bindings.begin(), info.bindings.end());

        if (!info.name.empty()) {
            device.set_debug_name({
                .type = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                .handle = reinterpret_cast<uint64>(layout->_handle),
                .name = info.name.c_str()
            });
        }
        return layout;
    }

    auto descriptor_layout_t::handle() const noexcept -> VkDescriptorSetLayout {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto descriptor_layout_t::device() const noexcept -> device_t& {
        IR_PROFILE_SCOPED();
        return _device.get();
    }

    auto descriptor_layout_t::bindings() const noexcept -> std::span<const descriptor_binding_t> {
        IR_PROFILE_SCOPED();
        return _bindings;
    }

    auto descriptor_layout_t::binding(uint32 index) const noexcept -> const descriptor_binding_t& {
        IR_PROFILE_SCOPED();
        return _bindings[index];
    }

    auto descriptor_layout_t::index() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        if (_bindings.empty()) {
            return 0;
        }
        return binding(0).set;
    }

    auto descriptor_layout_t::is_dynamic() const noexcept -> bool {
        IR_PROFILE_SCOPED();
        return !_bindings.empty() && _bindings.back().is_dynamic;
    }
}
