#include <iris/nvidia/ngx_wrapper.hpp>

#include <iris/gfx/descriptor_set.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/instance.hpp>
#include <iris/gfx/device.hpp>
#include <iris/gfx/buffer.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/queue.hpp>

#define IRIS_NVIDIA_PROJECT_ID "5F9FC80E-4C3D-4327-B4ED-86D84BC46457"
#define IRIS_NVIDIA_PROJECT_VERSION "0.1.0"
#define IRIS_NVIDIA_PROJECT_LOGS L"./logs"

namespace ir {
    static auto as_ngx_resource(const ir::image_t& image) noexcept -> NVSDK_NGX_Resource_VK {
        IR_PROFILE_SCOPED();
        const auto handle = image.handle();
        const auto view = image.view().handle();
        const auto format = as_enum_counterpart(image.format());
        const auto subresource = VkImageSubresourceRange {
            .aspectMask = static_cast<VkImageAspectFlags>(as_enum_counterpart(image.view().aspect())),
            .baseMipLevel = 0,
            .levelCount = image.levels(),
            .baseArrayLayer = 0,
            .layerCount = image.layers()
        };
        const auto width = image.width();
        const auto height = image.height();
        return NVSDK_NGX_Resource_VK {
            .Resource = {
                .ImageViewInfo = {
                    .ImageView = view,
                    .Image = handle,
                    .SubresourceRange = subresource,
                    .Format = format,
                    .Width = width,
                    .Height = height,
                },
            },
            .Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
            .ReadWrite = (image.usage() & image_usage_t::e_storage) == image_usage_t::e_storage,
        };
    }

    ngx_wrapper_t::ngx_wrapper_t(device_t& device) noexcept
        : _device(std::ref(device)) {
        IR_PROFILE_SCOPED();
    }

    ngx_wrapper_t::~ngx_wrapper_t() noexcept {
        IR_PROFILE_SCOPED();
        shutdown_dlss();
        NVSDK_NGX_VULKAN_DestroyParameters(_parameters);
        NVSDK_NGX_VULKAN_Shutdown1(nullptr);
    }

    auto ngx_wrapper_t::make(device_t& device) noexcept -> std::unique_ptr<self> {
        IR_PROFILE_SCOPED();
        auto ngx = std::make_unique<self>(device);

        if (!fs::exists(IRIS_NVIDIA_PROJECT_LOGS)) {
            fs::create_directories(IRIS_NVIDIA_PROJECT_LOGS);
        }
        IR_ASSERT(NVSDK_NGX_SUCCEED(
            NVSDK_NGX_VULKAN_Init_with_ProjectID(
                IRIS_NVIDIA_PROJECT_ID,
                NVSDK_NGX_ENGINE_TYPE_CUSTOM,
                IRIS_NVIDIA_PROJECT_VERSION,
                IRIS_NVIDIA_PROJECT_LOGS,
                device.instance().handle(),
                device.gpu(),
                device.handle())),
            "NVIDIA NGX: initialization failed"
        );

        IR_ASSERT(NVSDK_NGX_SUCCEED(
            NVSDK_NGX_VULKAN_GetCapabilityParameters(&ngx->_parameters)),
            "NVIDIA NGX: capability query failed"
        );

        auto is_driver_update_required = 0_i32;
        auto min_driver_version_major = 0_u32;
        auto min_driver_version_minor = 0_u32;

        IR_ASSERT(NVSDK_NGX_SUCCEED(
            ngx->_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &is_driver_update_required)),
            "NVIDIA NGX: driver update check failed"
        );
        IR_ASSERT(NVSDK_NGX_SUCCEED(
            ngx->_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &min_driver_version_major)),
            "NVIDIA NGX: driver version check failed"
        );
        IR_ASSERT(NVSDK_NGX_SUCCEED(
            ngx->_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &min_driver_version_minor)),
            "NVIDIA NGX: driver version check failed"
        );
        if (is_driver_update_required) {
            IR_LOG_CRITICAL(
                device.logger(),
                "NVIDIA NGX: driver update required ({}.{})",
                min_driver_version_major,
                min_driver_version_minor
            );
            IR_ASSERT(false, "NVIDIA NGX: driver update required");
        }

        auto is_dlss_available = 0_i32;
        IR_ASSERT(NVSDK_NGX_SUCCEED(
            ngx->_parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &is_dlss_available)),
            "NVIDIA NGX: DLSS not available"
        );
        IR_ASSERT(is_dlss_available, "NVIDIA NGX: DLSS not available");
        return ngx;
    }

    auto ngx_wrapper_t::initialize_dlss(const dlss_main_view_info_t& info) noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto dlss_feature_flags =
            (info.is_hdr ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0) |
            (info.is_reverse_depth ? NVSDK_NGX_DLSS_Feature_Flags_DepthInverted : 0) |
            (info.enable_auto_exposure ? NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0);

        auto dlss_create_parameters = NVSDK_NGX_DLSS_Create_Params();
        dlss_create_parameters.Feature.InWidth = info.render_resolution.x;
        dlss_create_parameters.Feature.InHeight = info.render_resolution.y;
        dlss_create_parameters.Feature.InTargetWidth = info.output_resolution.x;
        dlss_create_parameters.Feature.InTargetHeight = info.output_resolution.y;
        dlss_create_parameters.Feature.InPerfQualityValue = static_cast<NVSDK_NGX_PerfQuality_Value>(info.quality);
        dlss_create_parameters.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | dlss_feature_flags;

        const auto preset = NVSDK_NGX_DLSS_Hint_Render_Preset_Default;
        NVSDK_NGX_Parameter_SetUI(_parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, preset);
        NVSDK_NGX_Parameter_SetUI(_parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, preset);
        NVSDK_NGX_Parameter_SetUI(_parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, preset);
        NVSDK_NGX_Parameter_SetUI(_parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, preset);

        _device.get().graphics_queue().submit([&](ir::command_buffer_t& commands) {
            IR_ASSERT(NVSDK_NGX_SUCCEED(
                NGX_VULKAN_CREATE_DLSS_EXT(
                    commands.handle(),
                    1,
                    1,
                    &_dlss,
                    _parameters,
                    &dlss_create_parameters)),
                "NVIDIA NGX: DLSS creation failed");
        });
    }

    auto ngx_wrapper_t::shutdown_dlss() noexcept -> void {
        IR_PROFILE_SCOPED();
        NVSDK_NGX_VULKAN_ReleaseFeature(_dlss);
    }

    auto ngx_wrapper_t::evaluate(const dlss_main_view_evaluate_info_t& info) noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto width = info.color.get().width();
        const auto height = info.color.get().height();

        auto ngx_color = as_ngx_resource(info.color.get());
        auto ngx_depth = as_ngx_resource(info.depth.get());
        auto ngx_velocity = as_ngx_resource(info.velocity.get());
        auto ngx_output = as_ngx_resource(info.output.get());

        auto dlss_evaluation_parameters = NVSDK_NGX_VK_DLSS_Eval_Params();
        dlss_evaluation_parameters.Feature.pInColor = &ngx_color;
        dlss_evaluation_parameters.Feature.pInOutput = &ngx_output;
        dlss_evaluation_parameters.Feature.InSharpness = 0.0f;
        dlss_evaluation_parameters.pInDepth = &ngx_depth;
        dlss_evaluation_parameters.pInMotionVectors = &ngx_velocity;
        dlss_evaluation_parameters.InJitterOffsetX = info.jitter_offset.x;
        dlss_evaluation_parameters.InJitterOffsetY = info.jitter_offset.y;
        dlss_evaluation_parameters.InRenderSubrectDimensions = { width, height };
        dlss_evaluation_parameters.InReset = info.reset;
        dlss_evaluation_parameters.InMVScaleX = info.motion_vector_scale.x;
        dlss_evaluation_parameters.InMVScaleY = info.motion_vector_scale.y;
        IR_ASSERT(NVSDK_NGX_SUCCEED(
            NGX_VULKAN_EVALUATE_DLSS_EXT(
                info.commands.get().handle(),
                _dlss,
                _parameters,
                &dlss_evaluation_parameters)),
            "NVIDIA NGX: DLSS evaluation failed");
    }

    auto make_ngx_feature_common_info() -> NVSDK_NGX_FeatureCommonInfo {
        IR_PROFILE_SCOPED();
        auto ngx_feature_common_info = NVSDK_NGX_FeatureCommonInfo();
        ngx_feature_common_info.LoggingInfo.LoggingCallback =
            [](const char* message, NVSDK_NGX_Logging_Level, NVSDK_NGX_Feature) {
                //spdlog::info("[NGX] {}", message);
            };
        ngx_feature_common_info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;
        return ngx_feature_common_info;
    }

    auto make_ngx_feature_discovery_info(const NVSDK_NGX_FeatureCommonInfo& common_info) -> NVSDK_NGX_FeatureDiscoveryInfo {
        IR_PROFILE_SCOPED();
        auto ngx_feature_discovery_info = NVSDK_NGX_FeatureDiscoveryInfo();
        ngx_feature_discovery_info.SDKVersion = NVSDK_NGX_Version_API;
        ngx_feature_discovery_info.FeatureID = NVSDK_NGX_Feature_SuperSampling;
        ngx_feature_discovery_info.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
        ngx_feature_discovery_info.Identifier.v.ProjectDesc.ProjectId = IRIS_NVIDIA_PROJECT_ID;
        ngx_feature_discovery_info.Identifier.v.ProjectDesc.EngineType = NVSDK_NGX_ENGINE_TYPE_CUSTOM;
        ngx_feature_discovery_info.Identifier.v.ProjectDesc.EngineVersion = IRIS_NVIDIA_PROJECT_VERSION;
        ngx_feature_discovery_info.ApplicationDataPath = IRIS_NVIDIA_PROJECT_LOGS;
        ngx_feature_discovery_info.FeatureInfo = &common_info;
        return ngx_feature_discovery_info;
    }

    auto dlss_scaling_ratio_from_preset(dlss_quality_preset_t preset) noexcept -> float32 {
        IR_PROFILE_SCOPED();
        switch (preset) {
            case dlss_quality_preset_t::e_performance: return dlss_preset_performance_scaling_ratio;
            case dlss_quality_preset_t::e_balanced: return dlss_preset_balanced_scaling_ratio;
            case dlss_quality_preset_t::e_quality: return dlss_preset_quality_scaling_ratio;
            case dlss_quality_preset_t::e_native: return dlss_preset_native_scaling_ratio;
        }
        IR_UNREACHABLE();
    }
}
