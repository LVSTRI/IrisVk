#include <iris/core/utilities.hpp>

#include <iris/gfx/instance.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

#include <array>

namespace ir {
    instance_t::instance_t() noexcept = default;

    instance_t::~instance_t() noexcept {
        IR_PROFILE_SCOPED();
        if (_debug_messenger) {
            vkDestroyDebugUtilsMessengerEXT(_handle, _debug_messenger, nullptr);
        }
        vkDestroyInstance(_handle, nullptr);
        IR_LOG_INFO(_logger, "instance destroyed");
    }

    auto instance_t::make(const instance_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto instance = arc_ptr<self>(new self());
        auto logger = spdlog::stdout_color_mt("instance");

        volkInitialize();
        IR_LOG_INFO(logger, "initialized volk");
        auto extensions = std::vector<const char*>(info.wsi_extensions);
#if !defined(IRIS_DEBUG)
        if (info.features.debug_markers) {
            IR_LOG_INFO(logger, "debug markers enabled");
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
#endif

        auto api_version = 0_u32;
        IR_VULKAN_CHECK(logger, vkEnumerateInstanceVersion(&api_version));

        auto application_info = VkApplicationInfo();
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pNext = nullptr;
        application_info.pApplicationName = "Iris";
        application_info.applicationVersion = VK_API_VERSION_1_3;
        application_info.pEngineName = "Iris";
        application_info.engineVersion = VK_API_VERSION_1_3;
        application_info.apiVersion = VK_API_VERSION_1_3;

        auto instance_info = VkInstanceCreateInfo();
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pNext = nullptr;
        instance_info.flags = {};
        instance_info.pApplicationInfo = &application_info;
#if defined(IRIS_DEBUG)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        const auto* validation_layer_extension = "VK_LAYER_KHRONOS_validation";
        instance_info.enabledLayerCount = 1;
        instance_info.ppEnabledLayerNames = &validation_layer_extension;

        constexpr auto validation_extensions = std::to_array({
            //VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
            VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
            VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
        });
        auto validation_features = VkValidationFeaturesEXT();
        validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        validation_features.pNext = nullptr;
        validation_features.enabledValidationFeatureCount = validation_extensions.size();
        validation_features.pEnabledValidationFeatures = validation_extensions.data();
        validation_features.disabledValidationFeatureCount = 0;
        validation_features.pDisabledValidationFeatures = nullptr;

        // TODO: conditionally enable this
        instance_info.pNext = &validation_features;
#endif
        instance_info.enabledExtensionCount = extensions.size();
        instance_info.ppEnabledExtensionNames = extensions.data();
        IR_VULKAN_CHECK(logger, vkCreateInstance(&instance_info, nullptr, &instance->_handle));
        volkLoadInstanceOnly(instance->_handle);

        IR_LOG_INFO(logger, "instance initialized");

#if defined(IRIS_DEBUG)
        {
            auto validation_info = VkDebugUtilsMessengerCreateInfoEXT();
            validation_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            validation_info.pNext = nullptr;
            validation_info.flags = {};
            validation_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            validation_info.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            validation_info.pfnUserCallback = [](
                VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                VkDebugUtilsMessageTypeFlagsEXT type,
                const VkDebugUtilsMessengerCallbackDataEXT* data,
                void* user_data
            ) -> VkBool32 {
                auto& logger = *static_cast<spdlog::logger*>(user_data);

                auto level = spdlog::level::level_enum();
                switch (severity) {
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                        level = spdlog::level::debug;
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                        level = spdlog::level::info;
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                        level = spdlog::level::warn;
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                        level = spdlog::level::err;
                        break;
                    default: IR_UNREACHABLE();
                }

                const auto* type_str = "";
                switch (type) {
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
                        type_str = "general";
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
                        type_str = "validation";
                        break;
                    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
                        type_str = "performance";
                        break;
                    default: IR_UNREACHABLE();
                }

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT &&
                    type == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                ) {
                    return false;
                }

                logger.log(level, "[{}] {}", type_str, data->pMessage);
                logger.flush();

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    IR_ASSERT(false, "validation error");
                }
                return false;
            };
            validation_info.pUserData = logger.get();
            IR_VULKAN_CHECK(logger, vkCreateDebugUtilsMessengerEXT(instance->_handle, &validation_info, nullptr, &instance->_debug_messenger));
            IR_LOG_INFO(logger, "validation layers initialized");
        }
#endif
        instance->_api_version = api_version;
        instance->_info = info;
        instance->_logger = std::move(logger);
        return instance;
    }

    auto instance_t::handle() const noexcept -> VkInstance {
        IR_PROFILE_SCOPED();
        return _handle;
    }

    auto instance_t::api_version() const noexcept -> uint32 {
        IR_PROFILE_SCOPED();
        return _api_version;
    }

    auto instance_t::info() const noexcept -> const instance_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info;
    }

    auto instance_t::logger() const noexcept -> spdlog::logger& {
        IR_PROFILE_SCOPED();
        return *_logger;
    }

    auto instance_t::enumerate_physical_devices() const noexcept -> std::vector<VkPhysicalDevice> {
        IR_PROFILE_SCOPED();
        auto count = uint32();
        IR_VULKAN_CHECK(_logger, vkEnumeratePhysicalDevices(_handle, &count, nullptr));
        auto devs = std::vector<VkPhysicalDevice>(count);
        IR_VULKAN_CHECK(_logger, vkEnumeratePhysicalDevices(_handle, &count, devs.data()));
        return devs;
    }
}
