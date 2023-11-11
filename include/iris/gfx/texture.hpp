#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <spdlog/spdlog.h>

#include <vector>
#include <memory>
#include <span>

namespace ir {
    enum class texture_format_t {
        // TODO
        e_ttf_bc1_rgb,
        e_ttf_bc3_rgba,
        e_ttf_bc4_r,
        e_ttf_bc5_rg,
        e_ttf_bc7_rgba,
    };

    struct texture_create_info_t {
        std::string name = {};
        texture_format_t format = {};
    };

    class texture_t : public enable_intrusive_refcount_t<texture_t> {
    public:
        using self = texture_t;

        texture_t() noexcept;
        ~texture_t() noexcept;

        IR_NODISCARD static auto make(
            device_t& device,
            std::span<const uint8> file,
            const texture_create_info_t& info
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD static auto make(
            device_t& device,
            const fs::path& file,
            const texture_create_info_t& info
        ) noexcept -> arc_ptr<self>;

        IR_NODISCARD auto image() const noexcept -> const image_t&;

        IR_NODISCARD auto info() const noexcept -> image_info_t;
        IR_NODISCARD auto info(const ir::sampler_t& sampler) const noexcept -> image_info_t;
        IR_NODISCARD auto device() const noexcept -> const device_t&;

    private:
        arc_ptr<const image_t> _image = {};

        texture_create_info_t _info = {};
        arc_ptr<device_t> _device;
    };
}
