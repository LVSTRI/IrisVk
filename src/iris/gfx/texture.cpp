#include <iris/gfx/device.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/buffer.hpp>
#include <iris/gfx/sampler.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/texture.hpp>

#include <mio/mmap.hpp>

#include <ktx.h>

namespace ir {
    texture_t::texture_t() noexcept = default;

    texture_t::~texture_t() noexcept = default;

    auto texture_t::make(device_t& device, std::span<const uint8> file, const texture_create_info_t& info) noexcept -> arc_ptr<self> {
        IR_PROFILE_SCOPED();
        auto texture = arc_ptr<self>(new self());
        auto ec = std::error_code();
        auto ktx = (ktxTexture2*)(nullptr);
        IR_ASSERT(!ktxTexture2_CreateFromMemory(
            file.data(),
            file.size(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &ktx
        ), "texture loading failure");
        if (ktxTexture2_NeedsTranscoding(ktx)) {
            IR_LOG_WARN(device.logger(), "texture needs transcoding, format: {}", as_underlying(info.format));
            const auto transcode_format = [&info]() {
                switch (info.format) {
                    case texture_format_t::e_ttf_bc1_rgb: return KTX_TTF_BC1_RGB;
                    case texture_format_t::e_ttf_bc3_rgba: return KTX_TTF_BC3_RGBA;
                    case texture_format_t::e_ttf_bc4_r: return KTX_TTF_BC4_R;
                    case texture_format_t::e_ttf_bc5_rg: return KTX_TTF_BC5_RG;
                    case texture_format_t::e_ttf_bc7_rgba: return KTX_TTF_BC7_RGBA;
                }
                IR_UNREACHABLE();
            }();
            IR_ASSERT(!ktxTexture2_TranscodeBasis(ktx, transcode_format, KTX_TF_HIGH_QUALITY), "transcode failure");
        }
        auto staging = buffer_t<uint8>::make(device, {
            .usage = buffer_usage_t::e_transfer_src,
            .flags = buffer_flag_t::e_mapped,
            .capacity = ktx->dataSize
        });
        staging.as_ref().insert(0, ktx->dataSize, ktx->pData);
        auto image = image_t::make(device, {
            .width = ktx->baseWidth,
            .height = ktx->baseHeight,
            .levels = ktx->numLevels,
            .usage = image_usage_t::e_sampled | image_usage_t::e_transfer_dst,
            .format = static_cast<resource_format_t>(ktx->vkFormat),
            .view = default_image_view_info,
        });
        auto sampler = sampler_t::make(device, info.sampler);

        // TODO: use the transfer queue
        device.graphics_queue().submit([&](command_buffer_t& cmd) {
            cmd.image_barrier({
                .image = *image,
                .source_stage = pipeline_stage_t::e_none,
                .dest_stage = pipeline_stage_t::e_transfer,
                .source_access = resource_access_t::e_none,
                .dest_access = resource_access_t::e_transfer_write,
                .old_layout = image_layout_t::e_undefined,
                .new_layout = image_layout_t::e_transfer_dst_optimal,
            });
            for (auto i = 0_u32; i < ktx->numLevels; ++i) {
                auto offset = 0_u64;
                ktxTexture_GetImageOffset(ktxTexture(ktx), i, 0, 0, &offset);
                cmd.copy_buffer_to_image(staging.as_const_ref().slice(offset), image.as_const_ref(), {
                    .level = i,
                });
            }
            cmd.image_barrier({
                .image = *image,
                .source_stage = pipeline_stage_t::e_transfer,
                .dest_stage = pipeline_stage_t::e_fragment_shader,
                .source_access = resource_access_t::e_transfer_write,
                .dest_access = resource_access_t::e_shader_read,
                .old_layout = image_layout_t::e_transfer_dst_optimal,
                .new_layout = image_layout_t::e_shader_read_only_optimal,
            });
        });
        texture->_image = std::move(image);
        texture->_sampler = std::move(sampler);
        texture->_info = info;
        texture->_device = device.as_intrusive_ptr();
        ktxTexture_Destroy(ktxTexture(ktx));
        return texture;
    }

    auto texture_t::sampler_info() const noexcept -> const sampler_create_info_t& {
        IR_PROFILE_SCOPED();
        return _info.sampler;
    }

    auto texture_t::image() const noexcept -> const image_t& {
        IR_PROFILE_SCOPED();
        return *_image;
    }

    auto texture_t::sampler() const noexcept -> const sampler_t& {
        IR_PROFILE_SCOPED();
        return *_sampler;
    }

    auto texture_t::info() const noexcept -> image_info_t {
        IR_PROFILE_SCOPED();
        return {
            .sampler = _sampler.as_const_ref().handle(),
            .view = _image.as_const_ref().view().handle(),
            .layout = image_layout_t::e_shader_read_only_optimal
        };
    }

    auto texture_t::device() const noexcept -> const device_t& {
        IR_PROFILE_SCOPED();
        return _device.as_const_ref();
    }
}
