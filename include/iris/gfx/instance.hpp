#pragma once

#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <memory>

namespace ir {
    struct instance_create_info_t {
        struct features_t {
            bool debug_markers = false;
        } features;
        std::vector<const char*> wsi_extensions;
    };

    class instance_t : public enable_intrusive_refcount_t {
    public:
        using self = instance_t;

        instance_t() noexcept;
        ~instance_t() noexcept;

        static auto make(const instance_create_info_t& info = {}) noexcept -> intrusive_atomic_ptr_t<self>;

    private:
        VkInstance _handle = {};
        VkDebugUtilsMessengerEXT _debug_messenger = {};
        uint32 _api_version = 0;

        std::shared_ptr<spdlog::logger> _logger;
    };
}
