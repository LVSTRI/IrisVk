#pragma once

#if defined(IRIS_NVIDIA_DLSS)

#include <iris/core/forwards.hpp>
#include <iris/core/intrusive_atomic_ptr.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/enums.hpp>
#include <iris/core/types.hpp>

#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_vk.h>

#include <volk.h>
#include <vulkan/vulkan.h>

#include <memory>

#include <glm/vec2.hpp>

namespace ir {
    enum class dlss_quality_preset_t {
        e_performance = NVSDK_NGX_PerfQuality_Value_MaxPerf,
        e_balanced = NVSDK_NGX_PerfQuality_Value_Balanced,
        e_quality = NVSDK_NGX_PerfQuality_Value_MaxQuality,
        e_native = NVSDK_NGX_PerfQuality_Value_DLAA,
    };

    constexpr static auto dlss_preset_native_scaling_ratio = 1.0f;
    constexpr static auto dlss_preset_quality_scaling_ratio = 0.67f;
    constexpr static auto dlss_preset_balanced_scaling_ratio = 0.58f;
    constexpr static auto dlss_preset_performance_scaling_ratio = 0.5f;

    struct dlss_main_view_info_t {
        glm::uvec2 render_resolution = {};
        glm::uvec2 output_resolution = {};
        dlss_quality_preset_t quality = {};
        float32 depth_scale = 1.0f;

        bool is_hdr = false;
        bool is_reverse_depth = false;
        bool enable_auto_exposure = false;
    };

    struct dlss_main_view_evaluate_info_t {
        std::reference_wrapper<command_buffer_t> commands;
        std::reference_wrapper<const image_t> color;
        std::reference_wrapper<const image_t> depth;
        std::reference_wrapper<const image_t> velocity;
        std::reference_wrapper<const image_t> output;
        glm::vec2 jitter_offset = {};
        glm::vec2 motion_vector_scale = { 1.0f, 1.0f };
        bool reset = false;
    };

    class ngx_wrapper_t {
    public:
        using self = ngx_wrapper_t;

        ngx_wrapper_t(device_t& device) noexcept;
        ~ngx_wrapper_t() noexcept;

        IR_NODISCARD static auto make(device_t& device) noexcept -> std::unique_ptr<self>;

        auto initialize_dlss(const dlss_main_view_info_t& info) noexcept -> void;

        auto evaluate(const dlss_main_view_evaluate_info_t& info) noexcept -> void;

    private:
        NVSDK_NGX_Parameter* _parameters = nullptr;
        NVSDK_NGX_Handle* _dlss = nullptr;

        std::reference_wrapper<device_t> _device;
    };

    auto make_ngx_feature_common_info() -> NVSDK_NGX_FeatureCommonInfo;
    auto make_ngx_feature_discovery_info(const NVSDK_NGX_FeatureCommonInfo& common_info) -> NVSDK_NGX_FeatureDiscoveryInfo;
}

#endif
