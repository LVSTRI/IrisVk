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
    class swapchain_t : public enable_intrusive_refcount_t {
    public:
        swapchain_t() noexcept;
        ~swapchain_t() noexcept;

    private:

    };
}
