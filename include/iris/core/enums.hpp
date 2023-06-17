#pragma once

#include <iris/core/forwards.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/types.hpp>
#include <iris/core/utilities.hpp>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <type_traits>

namespace ir {
    namespace det {
        template <typename>
        struct native_enum_counterpart_type_t;

#define IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(E, T)                   \
    template <> struct native_enum_counterpart_type_t<E> { using type = T; }; \
    template <> struct native_enum_counterpart_type_t<T> { using type = E; }

        template <typename E>
        struct native_enum_string_func_t {
            constexpr auto operator ()(E) const noexcept -> auto {
                return nullptr;
            }
        };

#define IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(E)             \
    template <> struct native_enum_string_func_t<E> {            \
        constexpr auto operator ()(E e) const noexcept -> auto { \
            return IR_CONCAT(string_, E)(e);                     \
        }                                                        \
    }

        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(sample_count_t, VkSampleCountFlagBits);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(image_usage_t, VkImageUsageFlagBits);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(resource_format_t, VkFormat);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(image_aspect_t, VkImageAspectFlagBits);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(image_layout_t, VkImageLayout);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(descriptor_type_t, VkDescriptorType);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(shader_stage_t, VkShaderStageFlagBits);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(dynamic_state_t, VkDynamicState);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(cull_mode_t, VkCullModeFlagBits);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(buffer_usage_t, VkBufferUsageFlagBits);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(memory_property_t, VkMemoryPropertyFlagBits);
        IR_NATIVE_ENUM_CONTERPART_TYPE_SPECIALIZATION(component_swizzle_t, VkComponentSwizzle);

        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkSampleCountFlagBits);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkImageUsageFlagBits);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkFormat);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkImageAspectFlagBits);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkImageLayout);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkDescriptorType);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkShaderStageFlagBits);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkDynamicState);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkCullModeFlagBits);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkBufferUsageFlagBits);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkMemoryPropertyFlagBits);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkColorSpaceKHR);
        IR_NATIVE_ENUM_STRING_FUNC_SPECIALIZATION(VkComponentSwizzle);

        template <typename E>
        using native_enum_counterpart_type = typename native_enum_counterpart_type_t<E>::type;
    }

    template <typename E>
    constexpr auto as_enum_counterpart(E e) noexcept -> det::native_enum_counterpart_type<E> {
        return static_cast<det::native_enum_counterpart_type<E>>(e);
    }

    template <typename E>
    concept is_native_enum = requires (E e) {
        { det::native_enum_string_func_t<E>()(e) };
    };

    template <typename E>
        requires (!is_native_enum<E>)
    constexpr auto as_string(E e) noexcept -> auto {
        return det::native_enum_string_func_t<det::native_enum_counterpart_type<E>>()(e);
    }

    template <typename E>
        requires (is_native_enum<E>)
    constexpr auto as_string(E e) noexcept -> auto {
        return det::native_enum_string_func_t<E>()(e);
    }

    // VkSampleCountFlagBits
    enum class sample_count_t : std::underlying_type_t<VkSampleCountFlagBits> {
        e_1 = VK_SAMPLE_COUNT_1_BIT,
        e_2 = VK_SAMPLE_COUNT_2_BIT,
        e_4 = VK_SAMPLE_COUNT_4_BIT,
        e_8 = VK_SAMPLE_COUNT_8_BIT,
        e_16 = VK_SAMPLE_COUNT_16_BIT,
        e_32 = VK_SAMPLE_COUNT_32_BIT,
        e_64 = VK_SAMPLE_COUNT_64_BIT,
    };

    // VkImageUsageFlagBits
    enum class image_usage_t {
        e_transfer_src = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        e_transfer_dst = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        e_sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
        e_storage = VK_IMAGE_USAGE_STORAGE_BIT,
        e_color_attachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        e_depth_stencil_attachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        e_transient_attachment = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        e_input_attachment = VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        e_video_decode_dst = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR,
        e_video_decode_src = VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
        e_video_decode_dpb = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
        e_fragment_density_map = VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT,
        e_fragment_shading_rate_attachment = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
#ifdef VK_ENABLE_BETA_EXTENSIONS
        e_video_encode_dst = VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
        e_video_encode_src = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR,
        e_video_encode_dpb = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR,
#endif
        e_attachment_feedback_loop = VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT,
        e_invocation_mask_huawei = VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI,
        e_sample_weight_qcom = VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM,
        e_sample_block_match_qcom = VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM,
        e_shading_rate_image_nv = VK_IMAGE_USAGE_SHADING_RATE_IMAGE_BIT_NV,
    };
    
    // VkFormat
    enum class resource_format_t : std::underlying_type_t<VkFormat> {
        e_undefined = VK_FORMAT_UNDEFINED,
        e_r4g4_unorm_pack8 = VK_FORMAT_R4G4_UNORM_PACK8,
        e_r4g4b4a4_unorm_pack16 = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
        e_b4g4r4a4_unorm_pack16 = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
        e_r5g6b5_unorm_pack16 = VK_FORMAT_R5G6B5_UNORM_PACK16,
        e_b5g6r5_unorm_pack16 = VK_FORMAT_B5G6R5_UNORM_PACK16,
        e_r5g5b5a1_unorm_pack16 = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
        e_b5g5r5a1_unorm_pack16 = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
        e_a1r5g5b5_unorm_pack16 = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
        e_r8_unorm = VK_FORMAT_R8_UNORM,
        e_r8_snorm = VK_FORMAT_R8_SNORM,
        e_r8_uscaled = VK_FORMAT_R8_USCALED,
        e_r8_sscaled = VK_FORMAT_R8_SSCALED,
        e_r8_uint = VK_FORMAT_R8_UINT,
        e_r8_sint = VK_FORMAT_R8_SINT,
        e_r8_srgb = VK_FORMAT_R8_SRGB,
        e_r8g8_unorm = VK_FORMAT_R8G8_UNORM,
        e_r8g8_snorm = VK_FORMAT_R8G8_SNORM,
        e_r8g8_uscaled = VK_FORMAT_R8G8_USCALED,
        e_r8g8_sscaled = VK_FORMAT_R8G8_SSCALED,
        e_r8g8_uint = VK_FORMAT_R8G8_UINT,
        e_r8g8_sint = VK_FORMAT_R8G8_SINT,
        e_r8g8_srgb = VK_FORMAT_R8G8_SRGB,
        e_r8g8b8_unorm = VK_FORMAT_R8G8B8_UNORM,
        e_r8g8b8_snorm = VK_FORMAT_R8G8B8_SNORM,
        e_r8g8b8_uscaled = VK_FORMAT_R8G8B8_USCALED,
        e_r8g8b8_sscaled = VK_FORMAT_R8G8B8_SSCALED,
        e_r8g8b8_uint = VK_FORMAT_R8G8B8_UINT,
        e_r8g8b8_sint = VK_FORMAT_R8G8B8_SINT,
        e_r8g8b8_srgb = VK_FORMAT_R8G8B8_SRGB,
        e_b8g8r8_unorm = VK_FORMAT_B8G8R8_UNORM,
        e_b8g8r8_snorm = VK_FORMAT_B8G8R8_SNORM,
        e_b8g8r8_uscaled = VK_FORMAT_B8G8R8_USCALED,
        e_b8g8r8_sscaled = VK_FORMAT_B8G8R8_SSCALED,
        e_b8g8r8_uint = VK_FORMAT_B8G8R8_UINT,
        e_b8g8r8_sint = VK_FORMAT_B8G8R8_SINT,
        e_b8g8r8_srgb = VK_FORMAT_B8G8R8_SRGB,
        e_r8g8b8a8_unorm = VK_FORMAT_R8G8B8A8_UNORM,
        e_r8g8b8a8_snorm = VK_FORMAT_R8G8B8A8_SNORM,
        e_r8g8b8a8_uscaled = VK_FORMAT_R8G8B8A8_USCALED,
        e_r8g8b8a8_sscaled = VK_FORMAT_R8G8B8A8_SSCALED,
        e_r8g8b8a8_uint = VK_FORMAT_R8G8B8A8_UINT,
        e_r8g8b8a8_sint = VK_FORMAT_R8G8B8A8_SINT,
        e_r8g8b8a8_srgb = VK_FORMAT_R8G8B8A8_SRGB,
        e_b8g8r8a8_unorm = VK_FORMAT_B8G8R8A8_UNORM,
        e_b8g8r8a8_snorm = VK_FORMAT_B8G8R8A8_SNORM,
        e_b8g8r8a8_uscaled = VK_FORMAT_B8G8R8A8_USCALED,
        e_b8g8r8a8_sscaled = VK_FORMAT_B8G8R8A8_SSCALED,
        e_b8g8r8a8_uint = VK_FORMAT_B8G8R8A8_UINT,
        e_b8g8r8a8_sint = VK_FORMAT_B8G8R8A8_SINT,
        e_b8g8r8a8_srgb = VK_FORMAT_B8G8R8A8_SRGB,
        e_a8b8g8r8_unorm_pack32 = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        e_a8b8g8r8_snorm_pack32 = VK_FORMAT_A8B8G8R8_SNORM_PACK32,
        e_a8b8g8r8_uscaled_pack32 = VK_FORMAT_A8B8G8R8_USCALED_PACK32,
        e_a8b8g8r8_sscaled_pack32 = VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
        e_a8b8g8r8_uint_pack32 = VK_FORMAT_A8B8G8R8_UINT_PACK32,
        e_a8b8g8r8_sint_pack32 = VK_FORMAT_A8B8G8R8_SINT_PACK32,
        e_a8b8g8r8_srgb_pack32 = VK_FORMAT_A8B8G8R8_SRGB_PACK32,
        e_a2r10g10b10_unorm_pack32 = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        e_a2r10g10b10_snorm_pack32 = VK_FORMAT_A2R10G10B10_SNORM_PACK32,
        e_a2r10g10b10_uscaled_pack32 = VK_FORMAT_A2R10G10B10_USCALED_PACK32,
        e_a2r10g10b10_sscaled_pack32 = VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
        e_a2r10g10b10_uint_pack32 = VK_FORMAT_A2R10G10B10_UINT_PACK32,
        e_a2r10g10b10_sint_pack32 = VK_FORMAT_A2R10G10B10_SINT_PACK32,
        e_a2b10g10r10_unorm_pack32 = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
        e_a2b10g10r10_snorm_pack32 = VK_FORMAT_A2B10G10R10_SNORM_PACK32,
        e_a2b10g10r10_uscaled_pack32 = VK_FORMAT_A2B10G10R10_USCALED_PACK32,
        e_a2b10g10r10_sscaled_pack32 = VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
        e_a2b10g10r10_uint_pack32 = VK_FORMAT_A2B10G10R10_UINT_PACK32,
        e_a2b10g10r10_sint_pack32 = VK_FORMAT_A2B10G10R10_SINT_PACK32,
        e_r16_unorm = VK_FORMAT_R16_UNORM,
        e_r16_snorm = VK_FORMAT_R16_SNORM,
        e_r16_uscaled = VK_FORMAT_R16_USCALED,
        e_r16_sscaled = VK_FORMAT_R16_SSCALED,
        e_r16_uint = VK_FORMAT_R16_UINT,
        e_r16_sint = VK_FORMAT_R16_SINT,
        e_r16_sfloat = VK_FORMAT_R16_SFLOAT,
        e_r16g16_unorm = VK_FORMAT_R16G16_UNORM,
        e_r16g16_snorm = VK_FORMAT_R16G16_SNORM,
        e_r16g16_uscaled = VK_FORMAT_R16G16_USCALED,
        e_r16g16_sscaled = VK_FORMAT_R16G16_SSCALED,
        e_r16g16_uint = VK_FORMAT_R16G16_UINT,
        e_r16g16_sint = VK_FORMAT_R16G16_SINT,
        e_r16g16_sfloat = VK_FORMAT_R16G16_SFLOAT,
        e_r16g16b16_unorm = VK_FORMAT_R16G16B16_UNORM,
        e_r16g16b16_snorm = VK_FORMAT_R16G16B16_SNORM,
        e_r16g16b16_uscaled = VK_FORMAT_R16G16B16_USCALED,
        e_r16g16b16_sscaled = VK_FORMAT_R16G16B16_SSCALED,
        e_r16g16b16_uint = VK_FORMAT_R16G16B16_UINT,
        e_r16g16b16_sint = VK_FORMAT_R16G16B16_SINT,
        e_r16g16b16_sfloat = VK_FORMAT_R16G16B16_SFLOAT,
        e_r16g16b16a16_unorm = VK_FORMAT_R16G16B16A16_UNORM,
        e_r16g16b16a16_snorm = VK_FORMAT_R16G16B16A16_SNORM,
        e_r16g16b16a16_uscaled = VK_FORMAT_R16G16B16A16_USCALED,
        e_r16g16b16a16_sscaled = VK_FORMAT_R16G16B16A16_SSCALED,
        e_r16g16b16a16_uint = VK_FORMAT_R16G16B16A16_UINT,
        e_r16g16b16a16_sint = VK_FORMAT_R16G16B16A16_SINT,
        e_r16g16b16a16_sfloat = VK_FORMAT_R16G16B16A16_SFLOAT,
        e_r32_uint = VK_FORMAT_R32_UINT,
        e_r32_sint = VK_FORMAT_R32_SINT,
        e_r32_sfloat = VK_FORMAT_R32_SFLOAT,
        e_r32g32_uint = VK_FORMAT_R32G32_UINT,
        e_r32g32_sint = VK_FORMAT_R32G32_SINT,
        e_r32g32_sfloat = VK_FORMAT_R32G32_SFLOAT,
        e_r32g32b32_uint = VK_FORMAT_R32G32B32_UINT,
        e_r32g32b32_sint = VK_FORMAT_R32G32B32_SINT,
        e_r32g32b32_sfloat = VK_FORMAT_R32G32B32_SFLOAT,
        e_r32g32b32a32_uint = VK_FORMAT_R32G32B32A32_UINT,
        e_r32g32b32a32_sint = VK_FORMAT_R32G32B32A32_SINT,
        e_r32g32b32a32_sfloat = VK_FORMAT_R32G32B32A32_SFLOAT,
        e_r64_uint = VK_FORMAT_R64_UINT,
        e_r64_sint = VK_FORMAT_R64_SINT,
        e_r64_sfloat = VK_FORMAT_R64_SFLOAT,
        e_r64g64_uint = VK_FORMAT_R64G64_UINT,
        e_r64g64_sint = VK_FORMAT_R64G64_SINT,
        e_r64g64_sfloat = VK_FORMAT_R64G64_SFLOAT,
        e_r64g64b64_uint = VK_FORMAT_R64G64B64_UINT,
        e_r64g64b64_sint = VK_FORMAT_R64G64B64_SINT,
        e_r64g64b64_sfloat = VK_FORMAT_R64G64B64_SFLOAT,
        e_r64g64b64a64_uint = VK_FORMAT_R64G64B64A64_UINT,
        e_r64g64b64a64_sint = VK_FORMAT_R64G64B64A64_SINT,
        e_r64g64b64a64_sfloat = VK_FORMAT_R64G64B64A64_SFLOAT,
        e_b10g11r11_ufloat_pack32 = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
        e_e5b9g9r9_ufloat_pack32 = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
        e_d16_unorm = VK_FORMAT_D16_UNORM,
        e_x8_d24_unorm_pack32 = VK_FORMAT_X8_D24_UNORM_PACK32,
        e_d32_sfloat = VK_FORMAT_D32_SFLOAT,
        e_s8_uint = VK_FORMAT_S8_UINT,
        e_d16_unorm_s8_uint = VK_FORMAT_D16_UNORM_S8_UINT,
        e_d24_unorm_s8_uint = VK_FORMAT_D24_UNORM_S8_UINT,
        e_d32_sfloat_s8_uint = VK_FORMAT_D32_SFLOAT_S8_UINT,
        e_bc1_rgb_unorm_block = VK_FORMAT_BC1_RGB_UNORM_BLOCK,
        e_bc1_rgb_srgb_block = VK_FORMAT_BC1_RGB_SRGB_BLOCK,
        e_bc1_rgba_unorm_block = VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
        e_bc1_rgba_srgb_block = VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
        e_bc2_unorm_block = VK_FORMAT_BC2_UNORM_BLOCK,
        e_bc2_srgb_block = VK_FORMAT_BC2_SRGB_BLOCK,
        e_bc3_unorm_block = VK_FORMAT_BC3_UNORM_BLOCK,
        e_bc3_srgb_block = VK_FORMAT_BC3_SRGB_BLOCK,
        e_bc4_unorm_block = VK_FORMAT_BC4_UNORM_BLOCK,
        e_bc4_snorm_block = VK_FORMAT_BC4_SNORM_BLOCK,
        e_bc5_unorm_block = VK_FORMAT_BC5_UNORM_BLOCK,
        e_bc5_snorm_block = VK_FORMAT_BC5_SNORM_BLOCK,
        e_bc6h_ufloat_block = VK_FORMAT_BC6H_UFLOAT_BLOCK,
        e_bc6h_sfloat_block = VK_FORMAT_BC6H_SFLOAT_BLOCK,
        e_bc7_unorm_block = VK_FORMAT_BC7_UNORM_BLOCK,
        e_bc7_srgb_block = VK_FORMAT_BC7_SRGB_BLOCK,
        e_etc2_r8g8b8_unorm_block = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
        e_etc2_r8g8b8_srgb_block = VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
        e_etc2_r8g8b8a1_unorm_block = VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
        e_etc2_r8g8b8a1_srgb_block = VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
        e_etc2_r8g8b8a8_unorm_block = VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
        e_etc2_r8g8b8a8_srgb_block = VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
        e_eac_r11_unorm_block = VK_FORMAT_EAC_R11_UNORM_BLOCK,
        e_eac_r11_snorm_block = VK_FORMAT_EAC_R11_SNORM_BLOCK,
        e_eac_r11g11_unorm_block = VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
        e_eac_r11g11_snorm_block = VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
        e_astc_4x4_unorm_block = VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
        e_astc_4x4_srgb_block = VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
        e_astc_5x4_unorm_block = VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
        e_astc_5x4_srgb_block = VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
        e_astc_5x5_unorm_block = VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
        e_astc_5x5_srgb_block = VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
        e_astc_6x5_unorm_block = VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
        e_astc_6x5_srgb_block = VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
        e_astc_6x6_unorm_block = VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
        e_astc_6x6_srgb_block = VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
        e_astc_8x5_unorm_block = VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
        e_astc_8x5_srgb_block = VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
        e_astc_8x6_unorm_block = VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
        e_astc_8x6_srgb_block = VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
        e_astc_8x8_unorm_block = VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
        e_astc_8x8_srgb_block = VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
        e_astc_10x5_unorm_block = VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
        e_astc_10x5_srgb_block = VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
        e_astc_10x6_unorm_block = VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
        e_astc_10x6_srgb_block = VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
        e_astc_10x8_unorm_block = VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
        e_astc_10x8_srgb_block = VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
        e_astc_10x10_unorm_block = VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
        e_astc_10x10_srgb_block = VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
        e_astc_12x10_unorm_block = VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
        e_astc_12x10_srgb_block = VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
        e_astc_12x12_unorm_block = VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
        e_astc_12x12_srgb_block = VK_FORMAT_ASTC_12x12_SRGB_BLOCK,
        e_g8b8g8r8_422_unorm = VK_FORMAT_G8B8G8R8_422_UNORM,
        e_b8g8r8g8_422_unorm = VK_FORMAT_B8G8R8G8_422_UNORM,
        e_g8_b8_r8_3plane_420_unorm = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        e_g8_b8r8_2plane_420_unorm = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        e_g8_b8_r8_3plane_422_unorm = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        e_g8_b8r8_2plane_422_unorm = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
        e_g8_b8_r8_3plane_444_unorm = VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
        e_r10x6_unorm_pack16 = VK_FORMAT_R10X6_UNORM_PACK16,
        e_r10x6g10x6_unorm_2pack16 = VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
        e_r10x6g10x6b10x6a10x6_unorm_4pack16 = VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
        e_g10x6b10x6g10x6r10x6_422_unorm_4pack16 = VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
        e_b10x6g10x6r10x6g10x6_422_unorm_4pack16 = VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
        e_g10x6_b10x6_r10x6_3plane_420_unorm_3pack16 = VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
        e_g10x6_b10x6r10x6_2plane_420_unorm_3pack16 = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
        e_g10x6_b10x6_r10x6_3plane_422_unorm_3pack16 = VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
        e_g10x6_b10x6r10x6_2plane_422_unorm_3pack16 = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
        e_g10x6_b10x6_r10x6_3plane_444_unorm_3pack16 = VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
        e_r12x4_unorm_pack16 = VK_FORMAT_R12X4_UNORM_PACK16,
        e_r12x4g12x4_unorm_2pack16 = VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
        e_r12x4g12x4b12x4a12x4_unorm_4pack16 = VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
        e_g12x4b12x4g12x4r12x4_422_unorm_4pack16 = VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
        e_b12x4g12x4r12x4g12x4_422_unorm_4pack16 = VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
        e_g12x4_b12x4_r12x4_3plane_420_unorm_3pack16 = VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
        e_g12x4_b12x4r12x4_2plane_420_unorm_3pack16 = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
        e_g12x4_b12x4_r12x4_3plane_422_unorm_3pack16 = VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
        e_g12x4_b12x4r12x4_2plane_422_unorm_3pack16 = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
        e_g12x4_b12x4_r12x4_3plane_444_unorm_3pack16 = VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
        e_g16b16g16r16_422_unorm = VK_FORMAT_G16B16G16R16_422_UNORM,
        e_b16g16r16g16_422_unorm = VK_FORMAT_B16G16R16G16_422_UNORM,
        e_g16_b16_r16_3plane_420_unorm = VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
        e_g16_b16r16_2plane_420_unorm = VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
        e_g16_b16_r16_3plane_422_unorm = VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
        e_g16_b16r16_2plane_422_unorm = VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
        e_g16_b16_r16_3plane_444_unorm = VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
        e_g8_b8r8_2plane_444_unorm = VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
        e_g10x6_b10x6r10x6_2plane_444_unorm_3pack16 = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16,
        e_g12x4_b12x4r12x4_2plane_444_unorm_3pack16 = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16,
        e_g16_b16r16_2plane_444_unorm = VK_FORMAT_G16_B16R16_2PLANE_444_UNORM,
        e_a4r4g4b4_unorm_pack16 = VK_FORMAT_A4R4G4B4_UNORM_PACK16,
        e_a4b4g4r4_unorm_pack16 = VK_FORMAT_A4B4G4R4_UNORM_PACK16,
        e_astc_4x4_sfloat_block = VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK,
        e_astc_5x4_sfloat_block = VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK,
        e_astc_5x5_sfloat_block = VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK,
        e_astc_6x5_sfloat_block = VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK,
        e_astc_6x6_sfloat_block = VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK,
        e_astc_8x5_sfloat_block = VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK,
        e_astc_8x6_sfloat_block = VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK,
        e_astc_8x8_sfloat_block = VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK,
        e_astc_10x5_sfloat_block = VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK,
        e_astc_10x6_sfloat_block = VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK,
        e_astc_10x8_sfloat_block = VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK,
        e_astc_10x10_sfloat_block = VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK,
        e_astc_12x10_sfloat_block = VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK,
        e_astc_12x12_sfloat_block = VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK,
        e_pvrtc1_2bpp_unorm_block_img = VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG,
        e_pvrtc1_4bpp_unorm_block_img = VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG,
        e_pvrtc2_2bpp_unorm_block_img = VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG,
        e_pvrtc2_4bpp_unorm_block_img = VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG,
        e_pvrtc1_2bpp_srgb_block_img = VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG,
        e_pvrtc1_4bpp_srgb_block_img = VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG,
        e_pvrtc2_2bpp_srgb_block_img = VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG,
        e_pvrtc2_4bpp_srgb_block_img = VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG,
        e_r16g16_s10_5_nv = VK_FORMAT_R16G16_S10_5_NV,
    };
    
    // VkImageAspectFlagBits
    enum class image_aspect_t : std::underlying_type_t<VkImageAspectFlagBits> {
        e_color = VK_IMAGE_ASPECT_COLOR_BIT,
        e_depth = VK_IMAGE_ASPECT_DEPTH_BIT,
        e_stencil = VK_IMAGE_ASPECT_STENCIL_BIT,
        e_metadata = VK_IMAGE_ASPECT_METADATA_BIT,
        e_plane_0 = VK_IMAGE_ASPECT_PLANE_0_BIT,
        e_plane_1 = VK_IMAGE_ASPECT_PLANE_1_BIT,
        e_plane_2 = VK_IMAGE_ASPECT_PLANE_2_BIT,
        e_none = VK_IMAGE_ASPECT_NONE,
        e_memory_plane_0 = VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT,
        e_memory_plane_1 = VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT,
        e_memory_plane_2 = VK_IMAGE_ASPECT_MEMORY_PLANE_2_BIT_EXT,
        e_memory_plane_3 = VK_IMAGE_ASPECT_MEMORY_PLANE_3_BIT_EXT,
    };
    
    // VkImageLayout
    enum class image_layout_t : std::underlying_type_t<VkImageLayout> {
        e_undefined = VK_IMAGE_LAYOUT_UNDEFINED,
        e_general = VK_IMAGE_LAYOUT_GENERAL,
        e_color_attachment_optimal = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        e_depth_stencil_attachment_optimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        e_depth_stencil_read_only_optimal = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        e_shader_read_only_optimal = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        e_transfer_src_optimal = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        e_transfer_dst_optimal = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        e_preinitialized = VK_IMAGE_LAYOUT_PREINITIALIZED,
        e_depth_read_only_stencil_attachment_optimal = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
        e_depth_attachment_stencil_read_only_optimal = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
        e_depth_attachment_optimal = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        e_depth_read_only_optimal = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
        e_stencil_attachment_optimal = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
        e_stencil_read_only_optimal = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL,
        e_read_only_optimal = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
        e_attachment_optimal = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        e_present_src = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        e_video_decode_dst = VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR,
        e_video_decode_src = VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR,
        e_video_decode_dpb = VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR,
        e_shared_present = VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR,
        e_fragment_density_map_optimal = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
        e_fragment_shading_rate_attachment_optimal = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
#ifdef VK_ENABLE_BETA_EXTENSIONS
        e_video_encode_dst = VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR,
        e_video_encode_src = VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR,
        e_video_encode_dpb = VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR,
#endif
        e_attachment_feedback_loop_optimal = VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT,
        e_shading_rate_optimal_nv = VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV,
    };
    
    // VkPipelineStageFlagBits2
    enum class pipeline_stage_t : VkPipelineStageFlagBits2 {
        e_none = VK_PIPELINE_STAGE_2_NONE,
        e_top_of_pipe = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        e_draw_indirect = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
        e_vertex_input = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
        e_vertex_shader = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
        e_tessellation_control_shader = VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT,
        e_tessellation_evaluation_shader = VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT,
        e_geometry_shader = VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT,
        e_fragment_shader = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        e_early_fragment_tests = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
        e_late_fragment_tests = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        e_color_attachment_output = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        e_compute_shader = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        e_all_transfer = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
        e_transfer = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        e_bottom_of_pipe = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        e_host = VK_PIPELINE_STAGE_2_HOST_BIT,
        e_all_graphics = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        e_all_commands = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        e_copy = VK_PIPELINE_STAGE_2_COPY_BIT,
        e_resolve = VK_PIPELINE_STAGE_2_RESOLVE_BIT,
        e_blit = VK_PIPELINE_STAGE_2_BLIT_BIT,
        e_clear = VK_PIPELINE_STAGE_2_CLEAR_BIT,
        e_index_input = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
        e_vertex_attribute_input = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT,
        e_pre_rasterization_shaders = VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT,
        e_video_decode = VK_PIPELINE_STAGE_2_VIDEO_DECODE_BIT_KHR,
#if defined(VK_ENABLE_BETA_EXTENSIONS)
        e_video_encode = VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR,
#endif
        e_transform_feedback = VK_PIPELINE_STAGE_2_TRANSFORM_FEEDBACK_BIT_EXT,
        e_conditional_rendering = VK_PIPELINE_STAGE_2_CONDITIONAL_RENDERING_BIT_EXT,
        e_command_preprocess_nv = VK_PIPELINE_STAGE_2_COMMAND_PREPROCESS_BIT_NV,
        e_fragment_shading_rate_attachment = VK_PIPELINE_STAGE_2_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
        e_shading_rate_image_nv = VK_PIPELINE_STAGE_2_SHADING_RATE_IMAGE_BIT_NV,
        e_acceleration_structure_build = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        e_ray_tracing_shader = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        e_ray_tracing_shader_nv = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_NV,
        e_acceleration_structure_build_nv = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
        e_fragment_density_process = VK_PIPELINE_STAGE_2_FRAGMENT_DENSITY_PROCESS_BIT_EXT,
        e_task_shader_nv = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_NV,
        e_mesh_shader_nv = VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_NV,
        e_task_shader = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
        e_mesh_shader = VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT,
        e_subpass_shading_huawei = VK_PIPELINE_STAGE_2_SUBPASS_SHADING_BIT_HUAWEI,
        e_invocation_mask_huawei = VK_PIPELINE_STAGE_2_INVOCATION_MASK_BIT_HUAWEI,
        e_acceleration_structure_copy = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_COPY_BIT_KHR,
        e_micromap_build = VK_PIPELINE_STAGE_2_MICROMAP_BUILD_BIT_EXT,
        e_cluster_culling_shader_huawei = VK_PIPELINE_STAGE_2_CLUSTER_CULLING_SHADER_BIT_HUAWEI,
        e_optical_flow_nv = VK_PIPELINE_STAGE_2_OPTICAL_FLOW_BIT_NV,
    };
    
    // VkAccessFlagBits2
    enum class resource_access_t : VkAccessFlagBits2 {
        e_none = VK_ACCESS_2_NONE,
        e_indirect_command_read = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
        e_index_read = VK_ACCESS_2_INDEX_READ_BIT,
        e_vertex_attribute_read = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT,
        e_uniform_read = VK_ACCESS_2_UNIFORM_READ_BIT,
        e_input_attachment_read = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
        e_shader_read = VK_ACCESS_2_SHADER_READ_BIT,
        e_shader_write = VK_ACCESS_2_SHADER_WRITE_BIT,
        e_color_attachment_read = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        e_color_attachment_write = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        e_depth_stencil_attachment_read = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        e_depth_stencil_attachment_write = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        e_transfer_read = VK_ACCESS_2_TRANSFER_READ_BIT,
        e_transfer_write = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        e_host_read = VK_ACCESS_2_HOST_READ_BIT,
        e_host_write = VK_ACCESS_2_HOST_WRITE_BIT,
        e_memory_read = VK_ACCESS_2_MEMORY_READ_BIT,
        e_memory_write = VK_ACCESS_2_MEMORY_WRITE_BIT,
        e_shader_sampled_read = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
        e_shader_storage_read = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
        e_shader_storage_write = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        e_video_decode_read = VK_ACCESS_2_VIDEO_DECODE_READ_BIT_KHR,
        e_video_decode_write = VK_ACCESS_2_VIDEO_DECODE_WRITE_BIT_KHR,
#if defined(VK_ENABLE_BETA_EXTENSIONS)
        e_video_encode_read = VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR,
        e_video_encode_write = VK_ACCESS_2_VIDEO_ENCODE_WRITE_BIT_KHR,
#endif
        e_transform_feedback_write = VK_ACCESS_2_TRANSFORM_FEEDBACK_WRITE_BIT_EXT,
        e_transform_feedback_counter_read = VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT,
        e_transform_feedback_counter_write = VK_ACCESS_2_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT,
        e_conditional_rendering_read = VK_ACCESS_2_CONDITIONAL_RENDERING_READ_BIT_EXT,
        e_command_preprocess_read_nv = VK_ACCESS_2_COMMAND_PREPROCESS_READ_BIT_NV,
        e_command_preprocess_write_nv = VK_ACCESS_2_COMMAND_PREPROCESS_WRITE_BIT_NV,
        e_fragment_shading_rate_attachment_read = VK_ACCESS_2_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR,
        e_shading_rate_image_read_nv = VK_ACCESS_2_SHADING_RATE_IMAGE_READ_BIT_NV,
        e_acceleration_structure_read = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        e_acceleration_structure_write = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        e_acceleration_structure_read_nv = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_NV,
        e_acceleration_structure_write_nv = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_NV,
        e_fragment_density_map_read = VK_ACCESS_2_FRAGMENT_DENSITY_MAP_READ_BIT_EXT,
        e_color_attachment_read_noncoherent = VK_ACCESS_2_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT,
        e_descriptor_buffer_read = VK_ACCESS_2_DESCRIPTOR_BUFFER_READ_BIT_EXT,
        e_invocation_mask_read_huawei = VK_ACCESS_2_INVOCATION_MASK_READ_BIT_HUAWEI,
        e_shader_binding_table_read = VK_ACCESS_2_SHADER_BINDING_TABLE_READ_BIT_KHR,
        e_micromap_read = VK_ACCESS_2_MICROMAP_READ_BIT_EXT,
        e_micromap_write = VK_ACCESS_2_MICROMAP_WRITE_BIT_EXT,
        e_optical_flow_read_nv = VK_ACCESS_2_OPTICAL_FLOW_READ_BIT_NV,
        e_optical_flow_write_nv = VK_ACCESS_2_OPTICAL_FLOW_WRITE_BIT_NV,
    };
    
    // VkDescriptorType
    enum class descriptor_type_t : std::underlying_type_t<VkDescriptorType> {
        e_sampler = VK_DESCRIPTOR_TYPE_SAMPLER,
        e_combined_image_sampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        e_sampled_image = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        e_storage_image = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        e_uniform_texel_buffer = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
        e_storage_texel_buffer = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
        e_uniform_buffer = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        e_storage_buffer = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        e_uniform_buffer_dynamic = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        e_storage_buffer_dynamic = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
        e_input_attachment = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        e_inline_uniform_block = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK,
        e_acceleration_structure = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        e_acceleration_structure_nv = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV,
        e_sample_weight_image_qcom = VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM,
        e_block_match_image_qcom = VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM,
        e_mutable = VK_DESCRIPTOR_TYPE_MUTABLE_EXT,
        e_mutable_valve = VK_DESCRIPTOR_TYPE_MUTABLE_VALVE,
    };
    
    // VkShaderStageFlagBits
    enum class shader_stage_t : std::underlying_type_t<VkShaderStageFlagBits> {
        e_vertex = VK_SHADER_STAGE_VERTEX_BIT,
        e_tessellation_control = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
        e_tessellation_evaluation = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        e_geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
        e_fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
        e_compute = VK_SHADER_STAGE_COMPUTE_BIT,
        e_all_graphics = VK_SHADER_STAGE_ALL_GRAPHICS,
        e_all = VK_SHADER_STAGE_ALL,
        e_raygen = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        e_any_hit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        e_closest_hit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        e_miss = VK_SHADER_STAGE_MISS_BIT_KHR,
        e_intersection = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
        e_callable = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        e_task = VK_SHADER_STAGE_TASK_BIT_EXT,
        e_mesh = VK_SHADER_STAGE_MESH_BIT_EXT,
        e_subpass_shading_huawei = VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI,
        e_cluster_culling_huawei = VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI,
        e_raygen_nv = VK_SHADER_STAGE_RAYGEN_BIT_NV,
        e_any_hit_nv = VK_SHADER_STAGE_ANY_HIT_BIT_NV,
        e_closest_hit_nv = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV,
        e_miss_nv = VK_SHADER_STAGE_MISS_BIT_NV,
        e_intersection_nv = VK_SHADER_STAGE_INTERSECTION_BIT_NV,
        e_callable_nv = VK_SHADER_STAGE_CALLABLE_BIT_NV,
        e_task_nv = VK_SHADER_STAGE_TASK_BIT_NV,
        e_mesh_nv = VK_SHADER_STAGE_MESH_BIT_NV,
    };
    
    // VkDynamicState
    enum class dynamic_state_t : std::underlying_type_t<VkDynamicState> {
        e_viewport = VK_DYNAMIC_STATE_VIEWPORT,
        e_scissor = VK_DYNAMIC_STATE_SCISSOR,
        e_line_width = VK_DYNAMIC_STATE_LINE_WIDTH,
        e_depth_bias = VK_DYNAMIC_STATE_DEPTH_BIAS,
        e_blend_constants = VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        e_depth_bounds = VK_DYNAMIC_STATE_DEPTH_BOUNDS,
        e_stencil_compare_mask = VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        e_stencil_write_mask = VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
        e_stencil_reference = VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        e_cull_mode = VK_DYNAMIC_STATE_CULL_MODE,
        e_front_face = VK_DYNAMIC_STATE_FRONT_FACE,
        e_primitive_topology = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        e_viewport_with_count = VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
        e_scissor_with_count = VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
        e_vertex_input_binding_stride = VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE,
        e_depth_test_enable = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        e_depth_write_enable = VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        e_depth_compare_op = VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
        e_depth_bounds_test_enable = VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
        e_stencil_test_enable = VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
        e_stencil_op = VK_DYNAMIC_STATE_STENCIL_OP,
        e_rasterizer_discard_enable = VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
        e_depth_bias_enable = VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
        e_primitive_restart_enable = VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
        e_viewport_w_scaling_nv = VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_NV,
        e_discard_rectangle = VK_DYNAMIC_STATE_DISCARD_RECTANGLE_EXT,
        e_discard_rectangle_enable = VK_DYNAMIC_STATE_DISCARD_RECTANGLE_ENABLE_EXT,
        e_discard_rectangle_mode = VK_DYNAMIC_STATE_DISCARD_RECTANGLE_MODE_EXT,
        e_sample_locations = VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_EXT,
        e_ray_tracing_pipeline_stack_size = VK_DYNAMIC_STATE_RAY_TRACING_PIPELINE_STACK_SIZE_KHR,
        e_viewport_shading_rate_palette_nv = VK_DYNAMIC_STATE_VIEWPORT_SHADING_RATE_PALETTE_NV,
        e_viewport_coarse_sample_order_nv = VK_DYNAMIC_STATE_VIEWPORT_COARSE_SAMPLE_ORDER_NV,
        e_exclusive_scissor_enable_nv = VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_ENABLE_NV,
        e_exclusive_scissor_nv = VK_DYNAMIC_STATE_EXCLUSIVE_SCISSOR_NV,
        e_fragment_shading_rate = VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR,
        e_line_stipple = VK_DYNAMIC_STATE_LINE_STIPPLE_EXT,
        e_vertex_input = VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
        e_patch_control_points = VK_DYNAMIC_STATE_PATCH_CONTROL_POINTS_EXT,
        e_logic_op = VK_DYNAMIC_STATE_LOGIC_OP_EXT,
        e_color_write_enable = VK_DYNAMIC_STATE_COLOR_WRITE_ENABLE_EXT,
        e_tessellation_domain_origin = VK_DYNAMIC_STATE_TESSELLATION_DOMAIN_ORIGIN_EXT,
        e_depth_clamp_enable = VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT,
        e_polygon_mode = VK_DYNAMIC_STATE_POLYGON_MODE_EXT,
        e_rasterization_samples = VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT,
        e_sample_mask = VK_DYNAMIC_STATE_SAMPLE_MASK_EXT,
        e_alpha_to_coverage_enable = VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT,
        e_alpha_to_one_enable = VK_DYNAMIC_STATE_ALPHA_TO_ONE_ENABLE_EXT,
        e_logic_op_enable = VK_DYNAMIC_STATE_LOGIC_OP_ENABLE_EXT,
        e_color_blend_enable = VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT,
        e_color_blend_equation = VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT,
        e_color_write_mask = VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT,
        e_rasterization_stream = VK_DYNAMIC_STATE_RASTERIZATION_STREAM_EXT,
        e_conservative_rasterization_mode = VK_DYNAMIC_STATE_CONSERVATIVE_RASTERIZATION_MODE_EXT,
        e_extra_primitive_overestimation_size = VK_DYNAMIC_STATE_EXTRA_PRIMITIVE_OVERESTIMATION_SIZE_EXT,
        e_depth_clip_enable = VK_DYNAMIC_STATE_DEPTH_CLIP_ENABLE_EXT,
        e_sample_locations_enable = VK_DYNAMIC_STATE_SAMPLE_LOCATIONS_ENABLE_EXT,
        e_color_blend_advanced = VK_DYNAMIC_STATE_COLOR_BLEND_ADVANCED_EXT,
        e_provoking_vertex_mode = VK_DYNAMIC_STATE_PROVOKING_VERTEX_MODE_EXT,
        e_line_rasterization_mode = VK_DYNAMIC_STATE_LINE_RASTERIZATION_MODE_EXT,
        e_line_stipple_enable = VK_DYNAMIC_STATE_LINE_STIPPLE_ENABLE_EXT,
        e_depth_clip_negative_one_to_one = VK_DYNAMIC_STATE_DEPTH_CLIP_NEGATIVE_ONE_TO_ONE_EXT,
        e_viewport_w_scaling_enable_nv = VK_DYNAMIC_STATE_VIEWPORT_W_SCALING_ENABLE_NV,
        e_viewport_swizzle_nv = VK_DYNAMIC_STATE_VIEWPORT_SWIZZLE_NV,
        e_coverage_to_color_enable_nv = VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_ENABLE_NV,
        e_coverage_to_color_location_nv = VK_DYNAMIC_STATE_COVERAGE_TO_COLOR_LOCATION_NV,
        e_coverage_modulation_mode_nv = VK_DYNAMIC_STATE_COVERAGE_MODULATION_MODE_NV,
        e_coverage_modulation_table_enable_nv = VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_ENABLE_NV,
        e_coverage_modulation_table_nv = VK_DYNAMIC_STATE_COVERAGE_MODULATION_TABLE_NV,
        e_shading_rate_image_enable_nv = VK_DYNAMIC_STATE_SHADING_RATE_IMAGE_ENABLE_NV,
        e_representative_fragment_test_enable_nv = VK_DYNAMIC_STATE_REPRESENTATIVE_FRAGMENT_TEST_ENABLE_NV,
        e_coverage_reduction_mode_nv = VK_DYNAMIC_STATE_COVERAGE_REDUCTION_MODE_NV,
        e_attachment_feedback_loop_enable = VK_DYNAMIC_STATE_ATTACHMENT_FEEDBACK_LOOP_ENABLE_EXT,
    };
    
    // VkCullModeFlagBits
    enum class cull_mode_t : std::underlying_type_t<VkCullModeFlagBits> {
        e_none = VK_CULL_MODE_NONE,
        e_front = VK_CULL_MODE_FRONT_BIT,
        e_back = VK_CULL_MODE_BACK_BIT,
        e_front_and_back = VK_CULL_MODE_FRONT_AND_BACK,
    };
    
    // VkBufferUsageFlagBits
    enum class buffer_usage_t : std::underlying_type_t<VkBufferUsageFlagBits> {
        e_transfer_src = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        e_transfer_dst = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        e_uniform_texel_buffer = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
        e_storage_texel_buffer = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
        e_uniform_buffer = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        e_storage_buffer = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        e_index_buffer = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        e_vertex_buffer = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        e_indirect_buffer = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        e_shader_device_address = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        e_video_decode_src = VK_BUFFER_USAGE_VIDEO_DECODE_SRC_BIT_KHR,
        e_video_decode_dst = VK_BUFFER_USAGE_VIDEO_DECODE_DST_BIT_KHR,
        e_transform_feedback_buffer = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_BUFFER_BIT_EXT,
        e_transform_feedback_counter_buffer = VK_BUFFER_USAGE_TRANSFORM_FEEDBACK_COUNTER_BUFFER_BIT_EXT,
        e_conditional_rendering = VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT,
        e_acceleration_structure_build_input_read_only = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        e_acceleration_structure_storage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        e_shader_binding_table = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
#ifdef VK_ENABLE_BETA_EXTENSIONS
        e_video_encode_dst = VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR,
        e_video_encode_src = VK_BUFFER_USAGE_VIDEO_ENCODE_SRC_BIT_KHR,
#endif
        e_sampler_descriptor_buffer = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT,
        e_resource_descriptor_buffer = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
        e_push_descriptors_descriptor_buffer = VK_BUFFER_USAGE_PUSH_DESCRIPTORS_DESCRIPTOR_BUFFER_BIT_EXT,
        e_micromap_build_input_read_only = VK_BUFFER_USAGE_MICROMAP_BUILD_INPUT_READ_ONLY_BIT_EXT,
        e_micromap_storage = VK_BUFFER_USAGE_MICROMAP_STORAGE_BIT_EXT,
        e_ray_tracing_bit_nv = VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
    };
    
    // VkMemoryPropertyFlagBits
    enum class memory_property_t : std::underlying_type_t<VkMemoryPropertyFlagBits> {
        e_device_local = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        e_host_visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        e_host_coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        e_host_cached = VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
        e_lazily_allocated = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,
        e_protected = VK_MEMORY_PROPERTY_PROTECTED_BIT,
        e_device_coherent_bit_amd = VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD,
        e_device_uncached_bit_amd = VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD,
        e_rdma_capable_bit_nv = VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV,
    };

    // VkComponentSwizzle
    enum class component_swizzle_t : std::underlying_type_t<VkComponentSwizzle> {
        e_identity = VK_COMPONENT_SWIZZLE_IDENTITY,
        e_zero = VK_COMPONENT_SWIZZLE_ZERO,
        e_one = VK_COMPONENT_SWIZZLE_ONE,
        e_r = VK_COMPONENT_SWIZZLE_R,
        e_g = VK_COMPONENT_SWIZZLE_G,
        e_b = VK_COMPONENT_SWIZZLE_B,
        e_a = VK_COMPONENT_SWIZZLE_A,
    };


    // constants
    constexpr auto external_subpass = VK_SUBPASS_EXTERNAL;
    constexpr auto lod_clamp_none = VK_LOD_CLAMP_NONE;
    constexpr auto remaining_mip_levels = VK_REMAINING_MIP_LEVELS;
    constexpr auto remaining_array_layers = VK_REMAINING_ARRAY_LAYERS;
    constexpr auto whole_size = VK_WHOLE_SIZE;
    constexpr auto attachment_unused = VK_ATTACHMENT_UNUSED;
    constexpr auto queue_family_ignored = VK_QUEUE_FAMILY_IGNORED;
    constexpr auto level_ignored = -1_u32;
    constexpr auto remaining_levels = VK_REMAINING_MIP_LEVELS;
    constexpr auto layer_ignored = -1_u32;
    constexpr auto remaining_layers = VK_REMAINING_ARRAY_LAYERS;
}
