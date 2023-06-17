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
    struct swapchain_create_info_t {

    };

    class swapchain_t : public enable_intrusive_refcount_t {
    public:
        using self = swapchain_t;

        swapchain_t() noexcept;
        ~swapchain_t() noexcept;

    private:

    };
}
