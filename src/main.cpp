#include <utilities.hpp>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cstring>
#include <ranges>
#include <vector>
#include <cstdio>
#include <array>

#define IRIS_LOG_INFO(msg, ...)                                     \
    do {                                                            \
        std::printf("[info]: " msg "\n" __VA_OPT__(,) __VA_ARGS__); \
    } while (false)

#define IRIS_LOG_ERROR(msg, ...)                                     \
    do {                                                             \
        std::printf("[error]: " msg "\n" __VA_OPT__(,) __VA_ARGS__); \
    } while (false)

#if defined(NDEBUG)
    #define IRIS_ASSERT(x) ((void)(x))
    #define IRIS_VULKAN_ASSERT(x) ((void)(x))
#else
    #define IRIS_ASSERT(x) assert(x)
    #define IRIS_VULKAN_ASSERT(x)                            \
    do {                                                     \
        const auto __result = x;                             \
        if (__result != VK_SUCCESS) {                        \
            IRIS_LOG_ERROR("%s", string_VkResult(__result)); \
            assert(false);                                   \
        }                                                    \
    } while (false)
#endif

// how many frames the CPU can submit before it has to wait
#define FRAMES_IN_FLIGHT 2

using namespace iris::literals;

static auto query_available_extensions() -> std::vector<VkExtensionProperties> {
    auto count = 0_u32;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    auto extensions = std::vector<VkExtensionProperties>(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
    return extensions;
}

static auto query_available_layers() -> std::vector<VkLayerProperties> {
    auto count = 0_u32;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    auto layers = std::vector<VkLayerProperties>(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    return layers;
}

static auto query_available_extensions(VkPhysicalDevice device) -> std::vector<VkExtensionProperties> {
    auto count = 0_u32;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    auto extensions = std::vector<VkExtensionProperties>(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    return extensions;
}

static auto make_shader_module(VkDevice device, std::string_view source) -> VkShaderModule {
    auto module_info = VkShaderModuleCreateInfo();
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.pNext = nullptr;
    module_info.flags = {};
    module_info.codeSize = source.size();
    module_info.pCode = reinterpret_cast<const iris::uint32*>(source.data());
    
    auto shader_module = VkShaderModule();
    IRIS_VULKAN_ASSERT(vkCreateShaderModule(device, &module_info, nullptr, &shader_module));
    return shader_module;
}

struct swapchain_t {
    VkSwapchainKHR handle = {};
    VkSurfaceKHR surface = {};
    VkFormat format = {};
    VkExtent2D extent = {};
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
};

struct swapchain_info_t {
    swapchain_t* old = nullptr;
};

static auto make_swapchain(
        VkInstance instance,
        VkPhysicalDevice physical_device,
        VkDevice device,
        GLFWwindow* window,
        const swapchain_info_t& info) noexcept -> swapchain_t {
    auto* old_swapchain = info.old;

    auto surface = VkSurfaceKHR();
    if (old_swapchain) {
        surface = old_swapchain->surface;
    } else {
        IRIS_VULKAN_ASSERT(glfwCreateWindowSurface(instance, window, nullptr, &surface));
        IRIS_LOG_INFO("surface created: %p", (const void*)surface);
    }

    auto capabilities = VkSurfaceCapabilitiesKHR();
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

    auto format_count = 0_u32;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
    auto formats = std::vector<VkSurfaceFormatKHR>(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats.data());

    auto present_mode_count = 0_u32;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
    auto present_modes = std::vector<VkPresentModeKHR>(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data());

    // choose surface format
    auto surface_format = formats[0];
    for (const auto& each: formats) {
        if (each.format == VK_FORMAT_B8G8R8A8_SRGB && each.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = each;
            break;
        }
    }
    IRIS_LOG_INFO(
        "surface format: %s, %s",
        string_VkFormat(surface_format.format),
        string_VkColorSpaceKHR(surface_format.colorSpace));

    // choose present mode
    auto present_mode = VK_PRESENT_MODE_FIFO_KHR;
    IRIS_ASSERT(std::ranges::any_of(present_modes, [present_mode](const auto each) {
        return each == present_mode;
    }));
    IRIS_LOG_INFO("present mode: %s", string_VkPresentModeKHR(present_mode));

    // choose extent
    auto surface_extent = capabilities.currentExtent;
    if (surface_extent.width == -1_u32 || surface_extent.height == -1_u32) {
        glfwGetFramebufferSize(
            window,
            reinterpret_cast<iris::int32*>(&surface_extent.width),
            reinterpret_cast<iris::int32*>(&surface_extent.height));
        surface_extent = {
            std::clamp(surface_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp(surface_extent.height, capabilities.minImageExtent.height,
                       capabilities.maxImageExtent.height)
        };
    }
    IRIS_LOG_INFO("extent: %ux%u", surface_extent.width, surface_extent.height);

    // choose image count
    // We need clamp(min + 1, min, max) because just using min may cause us to wait before we can acquire
    // the next image, due to the driver internally using the image
    auto image_count = capabilities.minImageCount + 1;
    image_count = std::clamp(
        image_count,
        capabilities.minImageCount,
        capabilities.maxImageCount ?
        capabilities.maxImageCount :
        -1_u32);
    IRIS_LOG_INFO("image count: %u", image_count);

    auto swapchain = VkSwapchainKHR();
    auto swapchain_info = VkSwapchainCreateInfoKHR();
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.pNext = nullptr;
    swapchain_info.flags = {};
    swapchain_info.surface = surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = surface_format.format;
    swapchain_info.imageColorSpace = surface_format.colorSpace;
    swapchain_info.imageExtent = surface_extent;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.queueFamilyIndexCount = 0;
    swapchain_info.pQueueFamilyIndices = nullptr;
    swapchain_info.preTransform = capabilities.currentTransform;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = present_mode;
    swapchain_info.clipped = true;
    if (old_swapchain) {
        swapchain_info.oldSwapchain = old_swapchain->handle;
    }
    IRIS_VULKAN_ASSERT(vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain));
    IRIS_LOG_INFO("swapchain created: %p", (const void*)swapchain);

    auto swapchain_images = std::vector<VkImage>();
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
    swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());
    IRIS_LOG_INFO("swapchain images: %u", image_count);

    // VkImageView: A view of a VkImage describes how and what to access of the image we are referencing, such as
    // selecting a specific level or layer, defining a swizzle for the channels, etc.
    auto swapchain_image_views = std::vector<VkImageView>();
    swapchain_image_views.resize(image_count);
    auto image_view_info = VkImageViewCreateInfo();
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.pNext = nullptr;
    image_view_info.flags = {};
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = surface_format.format;
    image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;
    // we need a VkImageView for each VkImage
    for (auto i = 0_u32; i < image_count; ++i) {
        image_view_info.image = swapchain_images[i];
        IRIS_VULKAN_ASSERT(vkCreateImageView(device, &image_view_info, nullptr, &swapchain_image_views[i]));
        IRIS_LOG_INFO("swapchain image view created: %p", (const void*)swapchain_image_views[i]);
    }

    return {
        swapchain,
        surface,
        surface_format.format,
        surface_extent,
        std::move(swapchain_images),
        std::move(swapchain_image_views)
    };
}

static auto find_memory_type(VkPhysicalDevice physical_device, iris::uint32 type, VkMemoryPropertyFlags properties) noexcept -> iris::uint32 {
    auto memory_properties = VkPhysicalDeviceMemoryProperties();
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    for (auto i = 0_u32; i < memory_properties.memoryTypeCount; ++i) {
        if ((type & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return -1_u32;
}

int main() {
    if (!glfwInit()) {
        IRIS_LOG_ERROR("failed to initialize glfw");
        return -1;
    }
    IRIS_DEFER([]() {
        glfwTerminate();
    });

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto* window = glfwCreateWindow(800, 600, "IrisVk", nullptr, nullptr);
    IRIS_DEFER([window]() {
        glfwDestroyWindow(window);
    });

    // instance
    auto instance = VkInstance();
    auto validation_layer = VkDebugUtilsMessengerEXT();
    {
        auto count = 0_u32;
        const auto** surface_extensions = glfwGetRequiredInstanceExtensions(&count);
        auto extensions = std::vector(surface_extensions, surface_extensions + count);
#if !defined(NDEBUG)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
        const auto available_extensions = query_available_extensions();
        const auto available_layers = query_available_layers();

        if (!std::ranges::all_of(extensions, [&](const auto* extension) {
            return std::ranges::any_of(available_extensions, [&](const auto& available) {
                return std::strcmp(extension, available.extensionName) == 0;
            });
        })) {
            IRIS_LOG_ERROR("required extensions are not available");
            return -1;
        }

#if !defined(NDEBUG)
        const auto layers = std::vector<const char*>{ "VK_LAYER_KHRONOS_validation" };
        if (!std::ranges::all_of(layers, [&](const auto* layer) {
            return std::ranges::any_of(available_layers, [&](const auto& available) {
                return std::strcmp(layer, available.layerName) == 0;
            });
        })) {
            IRIS_LOG_ERROR("required layers are not available");
            return -1;
        }
#endif

        auto application_info = VkApplicationInfo();
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pNext = nullptr;
        application_info.pApplicationName = "IrisVk";
        application_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        application_info.pEngineName = "IrisVk";
        application_info.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        application_info.apiVersion = VK_API_VERSION_1_3;

        auto instance_info = VkInstanceCreateInfo();
        instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_info.pNext = nullptr;
        instance_info.flags = {};
        instance_info.pApplicationInfo = &application_info;
#if !defined(NDEBUG)
        instance_info.enabledLayerCount = layers.size();
        instance_info.ppEnabledLayerNames = layers.data();
        IRIS_LOG_INFO("validation layer enabled");
#else
        instance_info.enabledLayerCount = 0;
        instance_info.ppEnabledLayerNames = nullptr;
#endif
        instance_info.enabledExtensionCount = extensions.size();
        instance_info.ppEnabledExtensionNames = extensions.data();

        IRIS_VULKAN_ASSERT(vkCreateInstance(&instance_info, nullptr, &instance));
        IRIS_LOG_INFO("instance created: %p", (const void*)instance);

#if !defined(NDEBUG)
        auto validation_layer_info = VkDebugUtilsMessengerCreateInfoEXT();
        validation_layer_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        validation_layer_info.pNext = nullptr;
        validation_layer_info.flags = {};
        validation_layer_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        validation_layer_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        validation_layer_info.pfnUserCallback = [](
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*) -> VkBool32 {
            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                IRIS_LOG_INFO("validation layer message: %s", data->pMessage);
            }
            IRIS_ASSERT(severity != VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
            return false;
        };
        validation_layer_info.pUserData = nullptr;

        const auto vkCreateDebugUtilsMessengerEXT =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        IRIS_VULKAN_ASSERT(
            vkCreateDebugUtilsMessengerEXT(instance, &validation_layer_info, nullptr, &validation_layer));
#endif
    }

    auto physical_device = VkPhysicalDevice();
    {
        auto physical_device_count = 0_u32;
        vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
        auto physical_devices = std::vector<VkPhysicalDevice>(physical_device_count);
        vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data());

        for (const auto each: physical_devices) {
            auto features = VkPhysicalDeviceFeatures();
            auto properties = VkPhysicalDeviceProperties();
            vkGetPhysicalDeviceFeatures(each, &features);
            vkGetPhysicalDeviceProperties(each, &properties);

            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                IRIS_LOG_INFO("physical device: %s", properties.deviceName);
                physical_device = each;
                break;
            }
        }
        IRIS_ASSERT(physical_device != VK_NULL_HANDLE);
    }

    auto queue_family = -1_u32;
    {
        auto queue_family_count = 0_u32;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        auto queue_families = std::vector<VkQueueFamilyProperties>(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

        for (auto i = 0_u32; i < queue_family_count; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                IRIS_LOG_INFO("queue family: %u", i);
                queue_family = i;
                break;
            }
        }
        IRIS_ASSERT(queue_family != -1_u32);
    }

    auto queue = VkQueue();
    auto device = VkDevice();
    {
        const auto queue_priority = 1.0f;
        auto queue_device_info = VkDeviceQueueCreateInfo();
        queue_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_device_info.pNext = nullptr;
        queue_device_info.flags = {};
        queue_device_info.queueFamilyIndex = queue_family;
        queue_device_info.queueCount = 1;
        queue_device_info.pQueuePriorities = &queue_priority;

        auto extensions = std::vector<const char*>{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        const auto available_extensions = query_available_extensions(physical_device);
        if (!std::ranges::all_of(extensions, [&available_extensions](const auto each) {
            return std::ranges::any_of(available_extensions, [each](const auto& other) {
                return std::strcmp(each, other.extensionName) == 0;
            });
        })) {
            IRIS_LOG_ERROR("required device extensions are not available");
            return -1;
        }

        auto features = VkPhysicalDeviceFeatures();
        features.samplerAnisotropy = true;

        auto device_info = VkDeviceCreateInfo();
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.pNext = nullptr;
        device_info.flags = {};
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_device_info;
        device_info.enabledLayerCount = 0;
        device_info.ppEnabledLayerNames = nullptr;
        device_info.enabledExtensionCount = extensions.size();
        device_info.ppEnabledExtensionNames = extensions.data();
        device_info.pEnabledFeatures = &features;
        IRIS_VULKAN_ASSERT(vkCreateDevice(physical_device, &device_info, nullptr, &device));
        IRIS_LOG_INFO("device created: %p", (const void*)device);

        vkGetDeviceQueue(device, queue_family, 0, &queue);
    }

    auto swapchain = make_swapchain(instance, physical_device, device, window, {});

    auto render_pass = VkRenderPass();
    {
        auto color_attachment = VkAttachmentDescription();
        color_attachment.flags = {};
        color_attachment.format = swapchain.format;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        auto color_attachment_reference = VkAttachmentReference();
        color_attachment_reference.attachment = 0;
        color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        auto subpass = VkSubpassDescription();
        subpass.flags = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_reference;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = nullptr;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;

        // 0 clue...
        auto dependency = VkSubpassDependency();
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = {};

        auto render_pass_info = VkRenderPassCreateInfo();
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.pNext = nullptr;
        render_pass_info.flags = {};
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        IRIS_VULKAN_ASSERT(vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass));
        IRIS_LOG_INFO("render pass created: %p", (const void*)render_pass);
    }

    // TODO: Use libshaderc to compile instead of manually compiling
    auto pipeline = VkPipeline();
    {
        // load file
        auto vertex_shader_file = iris::whole_file("../shaders/0.1/main.vert.spv", true);
        auto fragment_shader_file = iris::whole_file("../shaders/0.1/main.frag.spv", true);

        // create shader modules
        auto vertex_shader_module = make_shader_module(device, vertex_shader_file);
        auto fragment_shader_module = make_shader_module(device, fragment_shader_file);
        IRIS_LOG_INFO("vertex module (\"%s\") created: %p", "../shaders/0.1/main.vert.spv", (const void*)vertex_shader_module);
        IRIS_LOG_INFO("fragment module (\"%s\") created: %p", "../shaders/0.1/main.frag.spv", (const void*)fragment_shader_module);

        auto vertex_stage_info = VkPipelineShaderStageCreateInfo();
        vertex_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_stage_info.pNext = nullptr;
        vertex_stage_info.flags = {};
        vertex_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_stage_info.module = vertex_shader_module;
        vertex_stage_info.pName = "main";
        vertex_stage_info.pSpecializationInfo = nullptr;

        auto fragment_stage_info = VkPipelineShaderStageCreateInfo();
        fragment_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_stage_info.pNext = nullptr;
        fragment_stage_info.flags = {};
        fragment_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_stage_info.module = fragment_shader_module;
        fragment_stage_info.pName = "main";
        fragment_stage_info.pSpecializationInfo = nullptr;

        auto vertex_inpit_binding = VkVertexInputBindingDescription();
        vertex_inpit_binding.binding = 0;
        vertex_inpit_binding.stride = sizeof(glm::vec3[2]);
        vertex_inpit_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        auto vertex_attributes = std::to_array({
            VkVertexInputAttributeDescription {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = 0
            },
            VkVertexInputAttributeDescription {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = sizeof(glm::vec3)
            },
        });

        // specifies the vertex format (similar to glVertexArrayAttribFormat)
        auto vertex_input_state_info = VkPipelineVertexInputStateCreateInfo();
        vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state_info.pNext = nullptr;
        vertex_input_state_info.flags = {};
        vertex_input_state_info.vertexBindingDescriptionCount = 1;
        vertex_input_state_info.pVertexBindingDescriptions = &vertex_inpit_binding;
        vertex_input_state_info.vertexAttributeDescriptionCount = vertex_attributes.size();
        vertex_input_state_info.pVertexAttributeDescriptions = vertex_attributes.data();

        auto input_assembly_state_info = VkPipelineInputAssemblyStateCreateInfo();
        input_assembly_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_info.pNext = nullptr;
        input_assembly_state_info.flags = {};
        input_assembly_state_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_info.primitiveRestartEnable = false;

        auto viewport = VkViewport();
        auto scissor = VkRect2D();

        auto viewport_state_info = VkPipelineViewportStateCreateInfo();
        viewport_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_info.pNext = nullptr;
        viewport_state_info.flags = {};
        viewport_state_info.viewportCount = 1;
        viewport_state_info.pViewports = &viewport;
        viewport_state_info.scissorCount = 1;
        viewport_state_info.pScissors = &scissor;

        auto rasterization_state_info = VkPipelineRasterizationStateCreateInfo();
        rasterization_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state_info.pNext = nullptr;
        rasterization_state_info.flags = {};
        rasterization_state_info.depthClampEnable = false;
        rasterization_state_info.rasterizerDiscardEnable = false;
        rasterization_state_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_state_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_state_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization_state_info.depthBiasEnable = false;
        rasterization_state_info.depthBiasConstantFactor = 0.0f;
        rasterization_state_info.depthBiasClamp = 0.0f;
        rasterization_state_info.depthBiasSlopeFactor = 0.0f;
        rasterization_state_info.lineWidth = 1.0f;

        auto multisampling_state_info = VkPipelineMultisampleStateCreateInfo();
        multisampling_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling_state_info.pNext = nullptr;
        multisampling_state_info.flags = {};
        multisampling_state_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_state_info.sampleShadingEnable = false;
        multisampling_state_info.minSampleShading = 1.0f;
        multisampling_state_info.pSampleMask = nullptr;
        multisampling_state_info.alphaToCoverageEnable = false;
        multisampling_state_info.alphaToOneEnable = false;

        auto depth_stencil_state_info = VkPipelineDepthStencilStateCreateInfo();
        depth_stencil_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil_state_info.pNext = nullptr;
        depth_stencil_state_info.flags = {};
        depth_stencil_state_info.depthTestEnable = false;
        depth_stencil_state_info.depthWriteEnable = false;
        depth_stencil_state_info.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_stencil_state_info.depthBoundsTestEnable = false;
        depth_stencil_state_info.stencilTestEnable = false;
        depth_stencil_state_info.front = {};
        depth_stencil_state_info.back = {};
        depth_stencil_state_info.minDepthBounds = 0.0f;
        depth_stencil_state_info.maxDepthBounds = 1.0f;

        auto color_blend_attachment_state_info = VkPipelineColorBlendAttachmentState();
        color_blend_attachment_state_info.blendEnable = false;
        color_blend_attachment_state_info.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state_info.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_state_info.colorBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state_info.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color_blend_attachment_state_info.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        color_blend_attachment_state_info.alphaBlendOp = VK_BLEND_OP_ADD;
        color_blend_attachment_state_info.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

        auto color_blending_state_info = VkPipelineColorBlendStateCreateInfo();
        color_blending_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blending_state_info.pNext = nullptr;
        color_blending_state_info.flags = {};
        color_blending_state_info.logicOpEnable = false;
        color_blending_state_info.logicOp = VK_LOGIC_OP_CLEAR;
        color_blending_state_info.attachmentCount = 1;
        color_blending_state_info.pAttachments = &color_blend_attachment_state_info;
        color_blending_state_info.blendConstants[0] = 0.0f;
        color_blending_state_info.blendConstants[1] = 0.0f;
        color_blending_state_info.blendConstants[2] = 0.0f;
        color_blending_state_info.blendConstants[3] = 0.0f;

        auto dynamic_states = std::to_array({
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        });
        auto dynamic_state_info = VkPipelineDynamicStateCreateInfo();
        dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_info.pNext = nullptr;
        dynamic_state_info.flags = {};
        dynamic_state_info.dynamicStateCount = dynamic_states.size();
        dynamic_state_info.pDynamicStates = dynamic_states.data();

        auto pipeline_layout_info = VkPipelineLayoutCreateInfo();
        pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_info.pNext = nullptr;
        pipeline_layout_info.flags = {};
        pipeline_layout_info.setLayoutCount = 0;
        pipeline_layout_info.pSetLayouts = nullptr;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;

        auto pipeline_layout = VkPipelineLayout();
        IRIS_VULKAN_ASSERT(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));
        IRIS_LOG_INFO("pipeline layout created: %p", (const void*)pipeline_layout);

        auto shader_stages = std::to_array({
            vertex_stage_info,
            fragment_stage_info
        });

        auto graphics_pipeline_info = VkGraphicsPipelineCreateInfo();
        graphics_pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphics_pipeline_info.pNext = nullptr;
        graphics_pipeline_info.flags = {};
        graphics_pipeline_info.stageCount = shader_stages.size();
        graphics_pipeline_info.pStages = shader_stages.data();
        graphics_pipeline_info.pVertexInputState = &vertex_input_state_info;
        graphics_pipeline_info.pInputAssemblyState = &input_assembly_state_info;
        graphics_pipeline_info.pTessellationState = nullptr;
        graphics_pipeline_info.pViewportState = &viewport_state_info;
        graphics_pipeline_info.pRasterizationState = &rasterization_state_info;
        graphics_pipeline_info.pMultisampleState = &multisampling_state_info;
        graphics_pipeline_info.pDepthStencilState = &depth_stencil_state_info;
        graphics_pipeline_info.pColorBlendState = &color_blending_state_info;
        graphics_pipeline_info.pDynamicState = &dynamic_state_info;
        graphics_pipeline_info.layout = pipeline_layout;
        graphics_pipeline_info.renderPass = render_pass;
        graphics_pipeline_info.subpass = 0;
        graphics_pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        graphics_pipeline_info.basePipelineIndex = -1;
        IRIS_VULKAN_ASSERT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_info, nullptr, &pipeline));
        IRIS_LOG_INFO("graphics pipeline created: %p", (const void*)pipeline);

        vkDestroyShaderModule(device, fragment_shader_module, nullptr);
        vkDestroyShaderModule(device, vertex_shader_module, nullptr);
    }

    auto framebuffers = std::vector<VkFramebuffer>(swapchain.image_views.size());
    for (auto i = 0_u32; i < swapchain.image_views.size(); ++i) {
        auto framebuffer_info = VkFramebufferCreateInfo();
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.pNext = nullptr;
        framebuffer_info.flags = {};
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &swapchain.image_views[i];
        framebuffer_info.width = swapchain.extent.width;
        framebuffer_info.height = swapchain.extent.height;
        framebuffer_info.layers = 1;
        IRIS_VULKAN_ASSERT(vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffers[i]));
        IRIS_LOG_INFO("framebuffer created: %p", (const void*)framebuffers[i]);
    }

    auto command_pool = VkCommandPool();
    {
        auto command_pool_info = VkCommandPoolCreateInfo();
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.pNext = nullptr;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_info.queueFamilyIndex = queue_family;
        IRIS_VULKAN_ASSERT(vkCreateCommandPool(device, &command_pool_info, nullptr, &command_pool));
        IRIS_LOG_INFO("command pool created: %p", (const void*)command_pool);
    }

    auto command_buffers = std::vector<VkCommandBuffer>(FRAMES_IN_FLIGHT);
    {
        auto command_buffer_info = VkCommandBufferAllocateInfo();
        command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_info.pNext = nullptr;
        command_buffer_info.commandPool = command_pool;
        command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_info.commandBufferCount = FRAMES_IN_FLIGHT;
        IRIS_VULKAN_ASSERT(vkAllocateCommandBuffers(device, &command_buffer_info, command_buffers.data()));
        IRIS_LOG_INFO("command buffer allocated: %p", command_buffers.data());
    }

    auto image_available = std::vector<VkSemaphore>(FRAMES_IN_FLIGHT);
    auto render_finished = std::vector<VkSemaphore>(FRAMES_IN_FLIGHT);
    auto previous_frame_finished = std::vector<VkFence>(FRAMES_IN_FLIGHT);
    {
        auto semaphore_info = VkSemaphoreCreateInfo();
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_info.pNext = nullptr;
        semaphore_info.flags = {};
        auto fence_info = VkFenceCreateInfo();
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.pNext = nullptr;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (auto i = 0_u32; i < FRAMES_IN_FLIGHT; ++i) {
            IRIS_VULKAN_ASSERT(vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available[i]));
            IRIS_LOG_INFO("image_available created: %p", (const void*)&image_available[i]);
            IRIS_VULKAN_ASSERT(vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished[i]));
            IRIS_LOG_INFO("render_finished created: %p", (const void*)&render_finished[i]);
            IRIS_VULKAN_ASSERT(vkCreateFence(device, &fence_info, nullptr, &previous_frame_finished[i]));
            IRIS_LOG_INFO("previous_frame_finished created: %p", (const void*)&previous_frame_finished[i]);
        }
    }

    auto vertices = std::to_array({
         0.0f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f,
    });

    auto vertex_buffer = VkBuffer();
    {
        auto buffer_info = VkBufferCreateInfo();
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.pNext = nullptr;
        buffer_info.flags = {};
        buffer_info.size = iris::size_bytes(vertices);
        buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        buffer_info.queueFamilyIndexCount = 0;
        buffer_info.pQueueFamilyIndices = nullptr;

        IRIS_VULKAN_ASSERT(vkCreateBuffer(device, &buffer_info, nullptr, &vertex_buffer));

        auto memory_requirements = VkMemoryRequirements();
        vkGetBufferMemoryRequirements(device, vertex_buffer, &memory_requirements);

        auto memory_type_index = find_memory_type(
            physical_device,
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        auto memory_allocate_info = VkMemoryAllocateInfo();
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.pNext = nullptr;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = memory_type_index;
        auto memory = VkDeviceMemory();
        IRIS_VULKAN_ASSERT(vkAllocateMemory(device, &memory_allocate_info, nullptr, &memory));
        IRIS_VULKAN_ASSERT(vkBindBufferMemory(device, vertex_buffer, memory, 0));

        void* data = nullptr;
        IRIS_VULKAN_ASSERT(vkMapMemory(device, memory, 0, memory_requirements.size, 0, &data));
        std::memcpy(data, vertices.data(), iris::size_bytes(vertices));
        vkUnmapMemory(device, memory);
    }

    const auto on_resize = [&]() {
        IRIS_LOG_INFO("swapchain out of date or suboptimal, recreating...");
        vkDeviceWaitIdle(device);
        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        for (auto image_view : swapchain.image_views) {
            vkDestroyImageView(device, image_view, nullptr);
        }
        swapchain = make_swapchain(instance, physical_device, device, window, {
            .old = &swapchain
        });
        framebuffers = std::vector<VkFramebuffer>(swapchain.image_views.size());
        for (auto i = 0_u32; i < swapchain.image_views.size(); ++i) {
            auto framebuffer_info = VkFramebufferCreateInfo();
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.pNext = nullptr;
            framebuffer_info.flags = {};
            framebuffer_info.renderPass = render_pass;
            framebuffer_info.attachmentCount = 1;
            framebuffer_info.pAttachments = &swapchain.image_views[i];
            framebuffer_info.width = swapchain.extent.width;
            framebuffer_info.height = swapchain.extent.height;
            framebuffer_info.layers = 1;
            IRIS_VULKAN_ASSERT(vkCreateFramebuffer(device, &framebuffer_info, nullptr, &framebuffers[i]));
            IRIS_LOG_INFO("framebuffer created: %p", (const void*)framebuffers[i]);
        }
    };

    auto frame_index = 0;
    while (!glfwWindowShouldClose(window)) {
        // wait for the previous frame to finish (?)
        vkWaitForFences(device, 1, &previous_frame_finished[frame_index], true, -1_u64);
        // reset fence to unsignaled state (because we will signal it this frame I guess?)
        vkResetFences(device, 1, &previous_frame_finished[frame_index]);

        // acquire an image from the swapchain and signal the queue when it's ready
        auto image_index = 0_u32;
        auto result = vkAcquireNextImageKHR(device, swapchain.handle, -1_u64, image_available[frame_index], VK_NULL_HANDLE, &image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            on_resize();
        }

        // reset the previously recorded commands, record new ones
        vkResetCommandBuffer(command_buffers[frame_index], 0);

        // begin rendering
        auto command_buffer_begin_info = VkCommandBufferBeginInfo();
        command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        command_buffer_begin_info.pNext = nullptr;
        command_buffer_begin_info.flags = {};
        command_buffer_begin_info.pInheritanceInfo = nullptr;
        IRIS_VULKAN_ASSERT(vkBeginCommandBuffer(command_buffers[frame_index], &command_buffer_begin_info));

        auto clear_color = VkClearValue();
        clear_color.color = { .float32 = { 0.0f, 0.0f, 0.0f, 1.0f } };

        auto viewport = VkViewport();
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = swapchain.extent.width;
        viewport.height = swapchain.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        auto scissor = VkRect2D();
        scissor.offset = { 0, 0 };
        scissor.extent = swapchain.extent;

        auto render_pass_begin_info = VkRenderPassBeginInfo();
        render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_begin_info.pNext = nullptr;
        render_pass_begin_info.renderPass = render_pass;
        render_pass_begin_info.framebuffer = framebuffers[image_index];
        render_pass_begin_info.renderArea.offset = {};
        render_pass_begin_info.renderArea.extent = swapchain.extent;
        render_pass_begin_info.clearValueCount = 1;
        render_pass_begin_info.pClearValues = &clear_color;
        vkCmdBeginRenderPass(command_buffers[frame_index], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command_buffers[frame_index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdSetViewport(command_buffers[frame_index], 0, 1, &viewport);
        vkCmdSetScissor(command_buffers[frame_index], 0, 1, &scissor);
        vkCmdBindVertexBuffers(command_buffers[frame_index], 0, 1, &vertex_buffer, iris::as_const_ptr(0_u64));
        vkCmdDraw(command_buffers[frame_index], 3, 1, 0, 0);
        vkCmdEndRenderPass(command_buffers[frame_index]);
        IRIS_VULKAN_ASSERT(vkEndCommandBuffer(command_buffers[frame_index]));

        // - at this point the GPU is still idle awaiting us to *actual* submit the recorded commands, we need to
        //   "push" the commands into a queue which will then be executed by the GPU in a sequential (??) manner.
        // - `pWaitDstStageMask` means: the corresponding wait semaphore will begin waiting *after* the commands execute
        //   the specified pipeline stage - in this case, the commands this submit will not wait for the semaphore before
        //   the pipeline reaches the specified stage (COLOR_ATTACHMENT_OUTPUT), which means we can execute the commands
        //   as long as we don't write to the swapchain image (because it isn't available yet)
        auto wait_stage = VkPipelineStageFlags(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        auto submit_info = VkSubmitInfo();
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.pNext = nullptr;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &image_available[frame_index];
        submit_info.pWaitDstStageMask = &wait_stage;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers[frame_index];
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &render_finished[frame_index];
        // we will wait in the next frame for this fence, because we need to have finished executing the commands
        // in the command buffer before we can reset it
        IRIS_VULKAN_ASSERT(vkQueueSubmit(queue, 1, &submit_info, previous_frame_finished[frame_index]));

        // we then send the "present" command to the GPU, making sure to **wait** for the queue submission to actually
        // finish, with the "render_finished" semaphore
        auto present_info = VkPresentInfoKHR();
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.pNext = nullptr;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_finished[frame_index];
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain.handle;
        present_info.pImageIndices = &image_index;
        present_info.pResults = nullptr;
        result = vkQueuePresentKHR(queue, &present_info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            on_resize();
        }

        frame_index = (frame_index + 1) % FRAMES_IN_FLIGHT;

        glfwPollEvents();
    }
    vkDeviceWaitIdle(device);

#if !defined(NDEBUG)
    const auto vkDestroyDebugUtilsMessengerEXT =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    vkDestroyDebugUtilsMessengerEXT(instance, validation_layer, nullptr);
#endif
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
