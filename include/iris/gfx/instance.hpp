#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <memory>

namespace ir {
    struct instance_features_t {
        bool debug_markers = false;
    };

    struct instance_create_info_t {
        instance_features_t features = {};
        std::vector<const char*> wsi_extensions;
    };

    class instance_t : public enable_intrusive_refcount_t<instance_t> {
    public:
        using self = instance_t;

        instance_t() noexcept;
        ~instance_t() noexcept;

        IR_NODISCARD static auto make(const instance_create_info_t& info = {}) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto handle() const noexcept -> VkInstance;
        IR_NODISCARD auto api_version() const noexcept -> uint32;

        IR_NODISCARD auto info() const noexcept -> const instance_create_info_t&;
        IR_NODISCARD auto logger() const noexcept -> const spdlog::logger&;

        IR_NODISCARD auto enumerate_physical_devices() const noexcept -> std::vector<VkPhysicalDevice>;

    private:
        VkInstance _handle = {};
        VkDebugUtilsMessengerEXT _debug_messenger = {};
        uint32 _api_version = 0;

        instance_create_info_t _info = {};
        std::shared_ptr<spdlog::logger> _logger;
    };
}
