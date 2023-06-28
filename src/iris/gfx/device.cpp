#include <iris/gfx/descriptor_layout.hpp>
#include <iris/gfx/descriptor_pool.hpp>
#include <iris/gfx/descriptor_set.hpp>
#include <iris/gfx/instance.hpp>
#include <iris/gfx/device.hpp>
#include <iris/gfx/queue.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace ir {
    template <typename T, typename U>
    static auto append_extension_chain(T& self, U* next) noexcept -> void {
        auto* old = self.pNext;
        self.pNext = next;
        next->pNext = old;
    }

    device_t::device_t() noexcept = default;

    device_t::~device_t() noexcept {
        IR_PROFILE_SCOPED();
        wait_idle();
        _descriptor_layouts.clear();
        _descriptor_sets.clear();
        _descriptor_pool.reset();
        _transfer.reset();
        _compute.reset();
        _graphics.reset();
        vmaDestroyAllocator(_allocator);
        IR_LOG_INFO(_logger, "allocator destroyed");
        vkDestroyDevice(_handle, nullptr);
        IR_LOG_INFO(_logger, "device destroyed");
    }

    auto device_t::make(
        const instance_t& instance,
        const device_create_info_t& info
    ) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto device = arc_ptr<self>(new self());
        auto logger = spdlog::stdout_color_mt("device");
        spdlog::create<spdlog::sinks::stdout_color_sink_mt>("cache");

        auto properties2 = VkPhysicalDeviceProperties2();
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = nullptr;
        auto memory_properties = VkPhysicalDeviceMemoryProperties2();
        memory_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        memory_properties.pNext = nullptr;

        // choose GPU
        for (auto gpu : instance.enumerate_physical_devices()) {
            vkGetPhysicalDeviceProperties2(gpu, &properties2);
            vkGetPhysicalDeviceMemoryProperties2(gpu, &memory_properties);
            IR_LOG_INFO(logger, "found GPU: {}", properties2.properties.deviceName);

            if (properties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
                properties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
                properties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
            ) {
                IR_LOG_INFO(logger, "acquired GPU: {}", properties2.properties.deviceName);
                const auto api_major = VK_API_VERSION_MAJOR(properties2.properties.apiVersion);
                const auto api_minor = VK_API_VERSION_MINOR(properties2.properties.apiVersion);
                const auto api_patch = VK_API_VERSION_PATCH(properties2.properties.apiVersion);
                IR_LOG_INFO(logger, "API version: {}.{}.{}", api_major, api_minor, api_patch);
                device->_gpu = gpu;
                break;
            }
        }
        IR_ASSERT(device->_gpu, "failed to find a suitable GPU");
        // device initialization
        {
            auto family_count = 0_u32;
            vkGetPhysicalDeviceQueueFamilyProperties2(device->_gpu, &family_count, nullptr);
            auto family_properties2 = std::vector<VkQueueFamilyProperties2>(family_count, VkQueueFamilyProperties2 {
                .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
                .pNext = nullptr
            });
            vkGetPhysicalDeviceQueueFamilyProperties2(device->_gpu, &family_count, family_properties2.data());

            auto queue_counts = std::vector<uint32>(family_count);
            auto queue_priorities = std::vector<std::vector<float32>>(family_count);
            const auto try_acquire_queue =
                [&](VkQueueFlags required, VkQueueFlags ignore, float32 priority) noexcept -> std::optional<queue_family_t> {
                    for (auto i = 0_u32; i < family_count; i++) {
                        const auto& properties = family_properties2[i].queueFamilyProperties;
                        if ((properties.queueFlags & ignore) != 0) {
                            continue;
                        }

                        const auto remaining = properties.queueCount - queue_counts[i];
                        if (remaining > 0 && (properties.queueFlags & required) == required) {
                            queue_priorities[i].emplace_back(priority);
                            return queue_family_t {
                                .family = i,
                                .index = queue_counts[i]++
                            };
                        }
                    }
                    return std::nullopt;
                };

            auto graphics_family = try_acquire_queue(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, 0, 0.5f);
            IR_ASSERT(graphics_family, "no graphics queue found");

            auto compute_family = try_acquire_queue(VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT, 0, 1.0f);
            if (!compute_family) {
                compute_family = try_acquire_queue(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 1.0f);
                if (!compute_family) {
                    compute_family = graphics_family;
                }
            }

            auto transfer_family = try_acquire_queue(VK_QUEUE_TRANSFER_BIT, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT, 0.5f);
            if (!transfer_family) {
                transfer_family = try_acquire_queue(VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT, 0.5f);
                if (!transfer_family) {
                    transfer_family = compute_family;
                }
            }

            auto queue_infos = std::vector<VkDeviceQueueCreateInfo>();
            for (auto i = 0_u32; i < family_count; ++i) {
                if (queue_counts[i] == 0) {
                    continue;
                }
                auto queue_info = VkDeviceQueueCreateInfo();
                queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_info.pNext = nullptr;
                queue_info.flags = {};
                queue_info.queueFamilyIndex = i;
                queue_info.queueCount = queue_counts[i];
                queue_info.pQueuePriorities = queue_priorities[i].data();
                queue_infos.emplace_back(queue_info);
            }

            auto extensions = std::vector<const char*>();
            extensions.emplace_back(VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME);
            extensions.emplace_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
            if (info.features.swapchain) {
                extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            }
            if (info.features.mesh_shader) {
                extensions.emplace_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
            }

            auto features_11 = VkPhysicalDeviceVulkan11Features();
            features_11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features_11.pNext = nullptr;
            features_11.storageBuffer16BitAccess = true;
            features_11.uniformAndStorageBuffer16BitAccess = true;
            features_11.storagePushConstant16 = true;
            features_11.variablePointersStorageBuffer = true;
            features_11.variablePointers = true;

            auto fragment_shading_rate_features = VkPhysicalDeviceFragmentShadingRateFeaturesKHR();
            fragment_shading_rate_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
            fragment_shading_rate_features.pNext = nullptr;
            fragment_shading_rate_features.pipelineFragmentShadingRate = true;
            append_extension_chain(features_11, &fragment_shading_rate_features);

            auto mesh_shader_features = VkPhysicalDeviceMeshShaderFeaturesEXT();
            mesh_shader_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
            mesh_shader_features.pNext = nullptr;
            mesh_shader_features.meshShader = true;
            mesh_shader_features.taskShader = true;
            if (info.features.mesh_shader) {
                append_extension_chain(features_11, &mesh_shader_features);
            }

            auto image64_atomics_features = VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT();
            image64_atomics_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT;
            image64_atomics_features.pNext = nullptr;
            image64_atomics_features.shaderImageInt64Atomics = true;
            append_extension_chain(features_11, &image64_atomics_features);

            auto features_12 = VkPhysicalDeviceVulkan12Features();
            features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            features_12.pNext = &features_11;
            features_12.samplerMirrorClampToEdge = true;
            features_12.drawIndirectCount = true;
            features_12.storageBuffer8BitAccess = true;
            features_12.uniformAndStorageBuffer8BitAccess = true;
            features_12.storagePushConstant8 = true;
            features_12.shaderBufferInt64Atomics = true;
            features_12.shaderSharedInt64Atomics = true;
            features_12.shaderFloat16 = true;
            features_12.shaderInt8 = true;
            features_12.descriptorIndexing = true;
            features_12.shaderInputAttachmentArrayDynamicIndexing = true;
            features_12.shaderUniformTexelBufferArrayDynamicIndexing = true;
            features_12.shaderStorageTexelBufferArrayDynamicIndexing = true;
            features_12.shaderUniformBufferArrayNonUniformIndexing = true;
            features_12.shaderSampledImageArrayNonUniformIndexing = true;
            features_12.shaderStorageBufferArrayNonUniformIndexing = true;
            features_12.shaderStorageImageArrayNonUniformIndexing = true;
            features_12.shaderInputAttachmentArrayNonUniformIndexing = true;
            features_12.shaderUniformTexelBufferArrayNonUniformIndexing = true;
            features_12.shaderStorageTexelBufferArrayNonUniformIndexing = true;
            features_12.descriptorBindingUniformBufferUpdateAfterBind = true;
            features_12.descriptorBindingSampledImageUpdateAfterBind = true;
            features_12.descriptorBindingStorageImageUpdateAfterBind = true;
            features_12.descriptorBindingStorageBufferUpdateAfterBind = true;
            features_12.descriptorBindingUniformTexelBufferUpdateAfterBind = true;
            features_12.descriptorBindingStorageTexelBufferUpdateAfterBind = true;
            features_12.descriptorBindingUpdateUnusedWhilePending = true;
            features_12.descriptorBindingPartiallyBound = true;
            features_12.descriptorBindingVariableDescriptorCount = true;
            features_12.runtimeDescriptorArray = true;
            features_12.samplerFilterMinmax = true;
            features_12.scalarBlockLayout = true;
            features_12.imagelessFramebuffer = true;
            features_12.uniformBufferStandardLayout = true;
            features_12.shaderSubgroupExtendedTypes = true;
            features_12.separateDepthStencilLayouts = true;
            features_12.hostQueryReset = true;
            features_12.timelineSemaphore = true;
            features_12.bufferDeviceAddress = true;
            features_12.bufferDeviceAddressCaptureReplay = true;
            features_12.bufferDeviceAddressMultiDevice = true;
            features_12.vulkanMemoryModel = true;
            features_12.vulkanMemoryModelDeviceScope = true;
            features_12.vulkanMemoryModelAvailabilityVisibilityChains = true;
            features_12.shaderOutputViewportIndex = true;
            features_12.shaderOutputLayer = true;
            features_12.subgroupBroadcastDynamicId = true;
            auto features_13 = VkPhysicalDeviceVulkan13Features();
            features_13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            features_13.pNext = &features_12;
            features_13.subgroupSizeControl = true;
            features_13.computeFullSubgroups = true;
            features_13.synchronization2 = true;
            features_13.dynamicRendering = true;
            features_13.maintenance4 = true;
            auto features2 = VkPhysicalDeviceFeatures2();
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &features_13;
            features2.features.fullDrawIndexUint32 = true;
            features2.features.imageCubeArray = true;
            features2.features.independentBlend = true;
            features2.features.sampleRateShading = true;
            features2.features.geometryShader = true;
            features2.features.multiDrawIndirect = true;
            features2.features.depthClamp = true;
            features2.features.depthBiasClamp = true;
            features2.features.depthBounds = true;
            features2.features.wideLines = true;
            features2.features.alphaToOne = true;
            features2.features.samplerAnisotropy = true;
            features2.features.textureCompressionBC = true;
            features2.features.pipelineStatisticsQuery = true;
            features2.features.vertexPipelineStoresAndAtomics = true;
            features2.features.fragmentStoresAndAtomics = true;
            features2.features.shaderUniformBufferArrayDynamicIndexing = true;
            features2.features.shaderSampledImageArrayDynamicIndexing = true;
            features2.features.shaderStorageBufferArrayDynamicIndexing = true;
            features2.features.shaderStorageImageArrayDynamicIndexing = true;
            features2.features.shaderFloat64 = true;
            features2.features.shaderInt64 = true;
            features2.features.shaderInt16 = true;
            features2.features.shaderResourceResidency = true;
            features2.features.shaderResourceMinLod = true;
            features2.features.sparseBinding = true;
            features2.features.sparseResidencyBuffer = true;
            features2.features.sparseResidencyImage2D = true;
            features2.features.sparseResidencyImage3D = true;
            features2.features.sparseResidencyAliased = true;
            features2.features.variableMultisampleRate = true;
            IR_LOG_INFO(logger, "device features enabled");

            auto device_info = VkDeviceCreateInfo();
            device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_info.pNext = &features2;
            device_info.pQueueCreateInfos = queue_infos.data();
            device_info.queueCreateInfoCount = queue_infos.size();
            device_info.enabledExtensionCount = extensions.size();
            device_info.ppEnabledExtensionNames = extensions.data();
            device_info.pEnabledFeatures = nullptr;
            IR_VULKAN_CHECK(logger, vkCreateDevice(device->_gpu, &device_info, nullptr, &device->_handle));
            IR_LOG_INFO(logger, "device initialized");
            volkLoadDevice(device->_handle);
            IR_LOG_INFO(logger, "device function table initialized");

            device->_graphics = queue_t::make(*device, { *graphics_family, queue_type_t::e_graphics });
            device->_compute = device->_graphics;
            device->_transfer = device->_graphics;
            if (graphics_family != compute_family) {
                device->_compute = queue_t::make(*device, { *compute_family, queue_type_t::e_compute });
            }
            if (transfer_family != graphics_family && transfer_family != compute_family) {
                device->_transfer = queue_t::make(*device, { *transfer_family, queue_type_t::e_transfer });
            }

            device->_properties = properties2;
            device->_memory_properties = memory_properties;
            device->_features = features2;
            device->_features_11 = features_11;
            device->_features_12 = features_12;
            device->_features_13 = features_13;
        }
        // VMA initialization
        {
            const auto& device_features = device->_features_12;
            auto vma_function_table = VmaVulkanFunctions();
            vma_function_table.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            vma_function_table.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

            auto vma_info = VmaAllocatorCreateInfo();
            vma_info.flags = 0;
            if (device_features.bufferDeviceAddress) {
                vma_info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
            }
            vma_info.physicalDevice = device->_gpu;
            vma_info.device = device->_handle;
            vma_info.preferredLargeHeapBlockSize = 0;
            vma_info.pAllocationCallbacks = nullptr;
            vma_info.pDeviceMemoryCallbacks = nullptr;
            vma_info.pHeapSizeLimit = nullptr;
            vma_info.pVulkanFunctions = &vma_function_table;
            vma_info.instance = instance.handle();
            vma_info.vulkanApiVersion = VK_API_VERSION_1_3;
            vma_info.pTypeExternalMemoryHandleTypes = nullptr;
            IR_VULKAN_CHECK(logger, vmaCreateAllocator(&vma_info, &device->_allocator));
            IR_LOG_INFO(logger, "main allocator initialized");
        }

        device->_frame_counter = master_frame_counter_t::make();

        device->_info = info;
        device->_instance = instance.as_intrusive_ptr();
        device->_logger = std::move(logger);

        device->_descriptor_pool = descriptor_pool_t::make(device.as_ref(), 128);

        return device;
    }

    auto device_t::handle() const noexcept -> VkDevice {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto device_t::gpu() const noexcept -> VkPhysicalDevice {
        IR_PROFILE_SCOPED();
        return _gpu;
    }

    auto device_t::allocator() const noexcept -> VmaAllocator {
        IR_PROFILE_SCOPED();
        return _allocator;
    }

    auto device_t::properties() const noexcept -> const VkPhysicalDeviceProperties& {
        IR_PROFILE_SCOPED();
        return _properties.properties;
    }

    auto device_t::memory_properties() const noexcept -> const VkPhysicalDeviceMemoryProperties& {
        IR_PROFILE_SCOPED();
        return _memory_properties.memoryProperties;
    }

    auto device_t::graphics_queue() noexcept -> queue_t& {
        IR_PROFILE_SCOPED();
        return *_graphics;
    }

    auto device_t::graphics_queue() const noexcept -> const queue_t& {
        IR_PROFILE_SCOPED();
        return *_graphics;
    }

    auto device_t::compute_queue() noexcept -> queue_t& {
        IR_PROFILE_SCOPED();
        return *_compute;
    }

    auto device_t::compute_queue() const noexcept -> const queue_t& {
        IR_PROFILE_SCOPED();
        return *_compute;
    }

    auto device_t::transfer_queue() noexcept -> queue_t& {
        IR_PROFILE_SCOPED();
        return *_transfer;
    }

    auto device_t::transfer_queue() const noexcept -> const queue_t& {
        IR_PROFILE_SCOPED();
        return *_transfer;
    }

    auto device_t::descriptor_pool() const noexcept -> const descriptor_pool_t& {
        IR_PROFILE_SCOPED();
        return *_descriptor_pool;
    }

    auto device_t::frame_counter() noexcept -> master_frame_counter_t& {
        IR_PROFILE_SCOPED();
        return *_frame_counter;
    }

    auto device_t::frame_counter() const noexcept -> const master_frame_counter_t& {
        IR_PROFILE_SCOPED();
        return *_frame_counter;
    }

    auto device_t::deletion_queue() noexcept -> deletion_queue_t& {
        IR_PROFILE_SCOPED();
        return _deletion_queue;
    }

    auto device_t::info() const noexcept -> const device_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto device_t::instance() const noexcept -> const instance_t& {
        IR_PROFILE_SCOPED();
        return *_instance;
    }

    auto device_t::logger() const noexcept -> spdlog::logger& {
        IR_PROFILE_SCOPED();
        return *_logger;
    }

    auto device_t::fetch_queue(const queue_family_t& family) const noexcept -> VkQueue {
        IR_PROFILE_SCOPED();
        auto queue = VkQueue();
        vkGetDeviceQueue(_handle, family.family, family.index, &queue);
        return queue;
    }

    auto device_t::wait_idle() const noexcept -> void {
        IR_PROFILE_SCOPED();
        IR_VULKAN_CHECK(_logger, vkDeviceWaitIdle(_handle));
    }

    auto device_t::resize_descriptor_pool(const akl::fast_hash_map<descriptor_type_t, uint32>& size) noexcept -> void {
        IR_PROFILE_SCOPED();
        _descriptor_pool = descriptor_pool_t::make(*this, size);
    }

    auto device_t::make_descriptor_layout(const std::vector<descriptor_binding_t>& bindings) noexcept -> arc_ptr<descriptor_layout_t> {
        IR_PROFILE_SCOPED();
        if (_descriptor_layouts.contains(bindings)) {
            return _descriptor_layouts.acquire(bindings);
        }
        return _descriptor_layouts.insert(bindings, descriptor_layout_t::make(*this, bindings));
    }

    auto device_t::acquire_descriptor_set(const descriptor_set_binding_t& bindings) noexcept -> arc_ptr<descriptor_set_t> {
        IR_PROFILE_SCOPED();
        if (_descriptor_sets.contains(bindings)) {
            return _descriptor_sets.acquire(bindings);
        }
        IR_LOG_WARN(logger(), "descriptor_set_t: cache miss");
        return nullptr;
    }

    auto device_t::register_descriptor_set(
        const descriptor_set_binding_t& bindings,
        arc_ptr<descriptor_set_t> set
    ) noexcept -> arc_ptr<descriptor_set_t> {
        IR_PROFILE_SCOPED();
        return _descriptor_sets.insert(bindings, std::move(set));
    }

    auto device_t::is_supported(device_feature_t feature) const noexcept -> bool {
        IR_PROFILE_SCOPED();
        switch (feature) {
            case device_feature_t::e_buffer_device_address: return _features_12.bufferDeviceAddress;
        }
        IR_UNREACHABLE();
    }

    auto device_t::tick() noexcept -> void {
        IR_PROFILE_SCOPED();
        frame_counter().tick();
        deletion_queue().tick();
        _descriptor_layouts.tick();
        _descriptor_sets.tick();
    }
}
