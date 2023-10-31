#include <application.hpp>

#include <iris/gfx/clear_value.hpp>

#include <iris/wsi/wsi_platform.hpp>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/integer.hpp>

#include <numeric>
#include <algorithm>

namespace test {
    static auto dispatch_work_group_size(uint32 size, uint32 wg) noexcept -> uint32 {
        return (size + wg - 1) / wg;
    }

    static auto dispatch_work_group_size(glm::uvec2 size = { 1, 1 }, glm::uvec2 wg = { 1, 1 }) noexcept -> glm::uvec2 {
        return glm::uvec2(
            (size.x + wg.x - 1) / wg.x,
            (size.y + wg.y - 1) / wg.y
        );
    }

    static auto dispatch_work_group_size(glm::uvec3 size = { 1, 1, 1 }, glm::uvec3 wg = { 1, 1, 1 }) noexcept -> glm::uvec3 {
        return glm::uvec3(
            (size.x + wg.x - 1) / wg.x,
            (size.y + wg.y - 1) / wg.y,
            (size.z + wg.z - 1) / wg.z
        );
    }

    static auto previous_power_2(uint32 value) noexcept -> uint32 {
        return 1 << (32 - std::countl_zero(value - 1));
    }

    static auto polar_to_cartesian(float32 elevation, float32 azimuth) noexcept -> glm::vec3 {
        elevation = glm::radians(elevation);
        azimuth = glm::radians(azimuth);
        return glm::normalize(-glm::vec3(
            std::cos(elevation) * std::cos(azimuth),
            std::sin(elevation),
            std::cos(elevation) * std::sin(azimuth)
        ));
    }

    application_t::application_t() noexcept
        : _camera(*_wsi) {
        IR_PROFILE_SCOPED();
        _initialize();
        _initialize_imgui();
        _initialize_sync();
        _initialize_visbuffer_pass();
        _initialize_vsm_pass();

        _load_models();
    }

    application_t::~application_t() noexcept {
        IR_PROFILE_SCOPED();
        _device->wait_idle();
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    auto application_t::run() noexcept -> void {
        IR_PROFILE_SCOPED();
        while (!_wsi->should_close()) {
            _update();
            _update_frame_data();
            _render();
            _frame_index = (_frame_index + 1) % frames_in_flight;
            IR_MARK_FRAME();
        }
    }

    auto application_t::_load_models() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto paths = std::to_array<fs::path>({
            "../models/compressed/bistro/bistro.glb",
        });
        auto models = std::vector<meshlet_model_t>();
        for (const auto& path : paths) {
            models.emplace_back(meshlet_model_t::make(path));
        }

        auto meshlets = std::vector<base_meshlet_t>();
        auto meshlet_instances = std::vector<meshlet_instance_t>();
        auto vertices = std::vector<meshlet_vertex_format_t>();
        auto indices = std::vector<uint32>();
        auto primitives = std::vector<uint8>();
        auto transforms = std::vector<transform_t>();
        auto materials = std::vector<material_t>();
        auto meshlet_offset = 0_u32;

        {
            auto meshlets_count = 0_u32;
            auto meshlet_instances_count = 0_u32;
            auto vertices_count = 0_u32;
            auto indices_count = 0_u32;
            auto primitives_count = 0_u32;
            auto transforms_count = 0_u32;
            auto materials_count = 0_u32;
            for (const auto& model : models) {
                meshlets_count += model.meshlets().size();
                meshlet_instances_count += model.meshlet_instances().size();
                vertices_count += model.vertices().size();
                indices_count += model.indices().size();
                primitives_count += model.primitives().size();
                transforms_count += model.transforms().size();
                materials_count += model.materials().size();
            }
            meshlets.reserve(meshlets_count);
            meshlet_instances.reserve(meshlet_instances_count);
            vertices.reserve(vertices_count);
            indices.reserve(indices_count);
            primitives.reserve(primitives_count);
            transforms.reserve(transforms_count);
            materials.reserve(materials_count);
        }

        for (auto& model : models) {
            for (const auto textures = model.textures(); const auto& [_, type, file] : textures) {
                _scene.textures.emplace_back(ir::texture_t::make(*_device, file, {
                    .format = [type = type]() {
                        switch (type) {
                            case texture_type_t::e_base_color:
                                return ir::texture_format_t::e_ttf_bc7_rgba;
                            case texture_type_t::e_normal:
                                return ir::texture_format_t::e_ttf_bc5_rg;
                        }
                        IR_UNREACHABLE();
                    }(),
                    .sampler = {
                        .filter = { ir::sampler_filter_t::e_linear },
                        .mip_mode = ir::sampler_mipmap_mode_t::e_linear,
                        .address_mode = { ir::sampler_address_mode_t::e_repeat },
                        .anisotropy = 16.0f,
                    }
                }));
            }

            for (const auto& meshlet : model.meshlets()) {
                auto base_meshlet = base_meshlet_t();
                base_meshlet.vertex_offset = meshlet.vertex_offset + vertices.size();
                base_meshlet.index_offset = meshlet.index_offset + indices.size();
                base_meshlet.primitive_offset = meshlet.primitive_offset + primitives.size();
                base_meshlet.index_count = meshlet.index_count;
                base_meshlet.primitive_count = meshlet.primitive_count;
                base_meshlet.aabb = meshlet.aabb;
                meshlets.emplace_back(base_meshlet);
            }
            for (auto instance : model.meshlet_instances()) {
                instance.meshlet_id += meshlet_offset;
                instance.instance_id += transforms.size();
                instance.material_id += materials.size();
                meshlet_instances.emplace_back(instance);
            }
            vertices.insert(vertices.end(), model.vertices().begin(), model.vertices().end());
            indices.insert(indices.end(), model.indices().begin(), model.indices().end());
            primitives.insert(primitives.end(), model.primitives().begin(), model.primitives().end());
            transforms.insert(transforms.end(), model.transforms().begin(), model.transforms().end());
            materials.insert(materials.end(), model.materials().begin(), model.materials().end());
            meshlet_offset += meshlets.size();
            model = {};
        }
        _scene.materials = std::move(materials);

        _buffer.meshlets = ir::upload_buffer<base_meshlet_t>(*_device, meshlets, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
        });
        _buffer.meshlet_instances = ir::upload_buffer<meshlet_instance_t>(*_device, meshlet_instances, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
        });
        _buffer.vertices = ir::upload_buffer<meshlet_vertex_format_t>(*_device, vertices, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
        });
        _buffer.indices = ir::upload_buffer<uint32>(*_device, indices, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
        });
        _buffer.primitives = ir::upload_buffer<uint8>(*_device, primitives, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
        });
        _buffer.transforms = ir::buffer_t<transform_t>::make(*_device, frames_in_flight, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
            .flags = ir::buffer_flag_t::e_mapped,
            .capacity = transforms.size(),
        });
        _buffer.materials = ir::buffer_t<material_t>::make(*_device, frames_in_flight, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
            .flags = ir::buffer_flag_t::e_mapped,
            .capacity = 4096,
        });
        _buffer.directional_lights = ir::buffer_t<directional_light_t>::make(*_device, frames_in_flight, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
            .flags = ir::buffer_flag_t::e_mapped,
            .capacity = IRIS_MAX_DIRECTIONAL_LIGHTS,
        });
        _buffer.views = ir::buffer_t<view_t>::make(*_device, frames_in_flight, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
            .flags = ir::buffer_flag_t::e_mapped,
            .capacity = 128,
        });
        _buffer.atomics = ir::buffer_t<uint64>::make(*_device, {
            .usage =
                ir::buffer_usage_t::e_storage_buffer |
                ir::buffer_usage_t::e_transfer_dst,
            .flags = ir::buffer_flag_t::e_resized,
            .capacity = 4096,
        });
        for (auto i = 0_u32; i < frames_in_flight; ++i) {
            _buffer.transforms[i]->insert(transforms);
        }
    }

    auto application_t::_current_frame_data() noexcept {
        IR_PROFILE_SCOPED();
        return std::forward_as_tuple(
            *_command_buffers[_frame_index],
            *_image_available[_frame_index],
            *_render_done[_frame_index],
            *_frame_fence[_frame_index]
        );
    }

    auto application_t::_current_frame_buffers() noexcept {
        IR_PROFILE_SCOPED();
        return std::forward_as_tuple(
            *_buffer.views[_frame_index],
            *_buffer.transforms[_frame_index]
        );
    }

    auto application_t::_render() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        if (_resize_main_viewport()) {
            return;
        }

        command_buffer.pool().reset();
        auto [image_index, should_resize] = _swapchain->acquire_next_image(image_available);
        if (should_resize) {
            _resize();
            return;
        }

        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();

        command_buffer.begin();
        _clear_buffer_pass();
        _visbuffer_pass();
        _visbuffer_resolve_pass();
        _visbuffer_dlss_pass();
        _visbuffer_tonemap_pass();
        _vsm_mark_visible_pages_pass();
        _gui_main_pass();
        _swapchain_copy_pass(image_index);
        command_buffer.end();

        _device->graphics_queue().submit({
            .command_buffers = { std::cref(command_buffer) },
            .wait_semaphores = {
                { std::cref(image_available), ir::pipeline_stage_t::e_transfer }
            },
            .signal_semaphores = {
                { std::cref(render_done), ir::pipeline_stage_t::e_none }
            },
        }, &frame_fence);

        should_resize = _device->graphics_queue().present({
            .swapchain = std::cref(*_swapchain),
            .wait_semaphores = { std::cref(render_done) },
            .image = image_index,
        });
        if (should_resize) {
            _resize();
            return;
        }
    }

    auto application_t::_update() noexcept -> void {
        IR_PROFILE_SCOPED();
        ir::wsi_platform_t::poll_events();
        const auto current_time = ch::steady_clock::now();
        _delta_time = ch::duration<float32>(current_time - _last_frame).count();
        _last_frame = current_time;
        auto [viewport_width, viewport_height] = _wsi->update_viewport();
        while (viewport_width == 0 || viewport_height == 0) {
            ir::wsi_platform_t::wait_events();
            std::tie(viewport_width, viewport_height) = _wsi->update_viewport();
        }
        if (_wsi->input().is_pressed_once(ir::mouse_t::e_right_button)) {
            _wsi->capture_cursor();
        }
        if (_wsi->input().is_released_once(ir::mouse_t::e_right_button)) {
            _wsi->release_cursor();
        }
        _wsi->input().capture();
        _state.performance.frame_times.pop_front();
        _state.performance.frame_times.push_back(_delta_time * 1000.0f);
        _camera.update(_delta_time);
        _device->tick();
    }

    auto application_t::_update_frame_data() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        frame_fence.wait();
        frame_fence.reset();

        const auto& [
            view_buffer,
            transform_buffer
        ] = _current_frame_buffers();

        auto& material_buffer = *_buffer.materials[_frame_index];
        auto& directional_light_buffer = *_buffer.directional_lights[_frame_index];
        auto& vsm_globals_buffer = *_vsm.globals_buffer[_frame_index];

        // write directional lights
        _state.directional_lights = std::vector<directional_light_t>(IRIS_MAX_DIRECTIONAL_LIGHTS);
        {
            static auto elevation = 240.0f;
            static auto azimuth = 30.0f;
            if (_wsi->input().is_pressed(ir::keyboard_t::e_up)) {
                elevation += 25.0f * _delta_time;
            }
            if (_wsi->input().is_pressed(ir::keyboard_t::e_down)) {
                elevation -= 25.0f * _delta_time;
            }
            if (_wsi->input().is_pressed(ir::keyboard_t::e_left)) {
                azimuth -= 25.0f * _delta_time;
            }
            if (_wsi->input().is_pressed(ir::keyboard_t::e_right)) {
                azimuth += 25.0f * _delta_time;
            }
            if (glm::epsilonEqual(elevation, 360.0f, 0.0001f)) {
                elevation += 0.0002f;
            }
            auto directional_light = directional_light_t();
            directional_light.direction = polar_to_cartesian(elevation, azimuth);
            directional_light.intensity = 1.0f;
            _state.directional_lights[0] = directional_light;
        }
        directional_light_buffer.insert(_state.directional_lights);

        // write main view
        auto views = std::vector<view_t>(128);
        {
            auto view = view_t();
            view.projection = _camera.projection();
            view.prev_projection = _visbuffer.prev_projection;
            view.inv_projection = glm::inverse(view.projection);
            view.inv_prev_projection = glm::inverse(view.prev_projection);
            view.view = _camera.view();
            view.prev_view = _visbuffer.prev_view;
            view.inv_view = glm::inverse(view.view);
            view.inv_prev_view = glm::inverse(view.prev_view);
            view.proj_view = view.projection * view.view;
            view.prev_proj_view = view.prev_projection * view.prev_view;
            view.inv_proj_view = glm::inverse(view.proj_view);
            view.inv_prev_proj_view = glm::inverse(view.prev_proj_view);
            view.eye_position = glm::make_vec4(_camera.position());

            const auto frustum = make_perspective_frustum(view.proj_view);
            std::memcpy(view.frustum, frustum.data(), sizeof(view.frustum));
            view.resolution = glm::vec2(_state.dlss.render_resolution);
            views[IRIS_MAIN_VIEW_INDEX] = view;

            _visbuffer.prev_projection = view.projection;
            _visbuffer.prev_view = view.view;
        }

        // write shadow views
        {
            const auto& sun_dir_light = _state.directional_lights[0];
            for (auto i = 0_u32; i < IRIS_VSM_CLIPMAP_COUNT; ++i) {
                const auto width = _state.vsm_global_data.first_width * (1 << i) / 2.0f;
                const auto center = glm::trunc(_camera.position());

                auto view = view_t();
                view.projection = glm::ortho(-width, width, -width, width, -2000.0f, 2000.0f);
                view.projection[1][1] *= -1.0f;
                view.inv_projection = glm::inverse(view.projection);
                view.view = glm::lookAt(
                    center + sun_dir_light.direction,
                    center,
                    glm::vec3(0.0f, 1.0f, 0.0f));
                view.inv_view = glm::inverse(view.view);
                view.stable_view = glm::lookAt(sun_dir_light.direction, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                view.inv_stable_view = glm::inverse(view.stable_view);
                view.proj_view = view.projection * view.view;
                view.inv_proj_view = glm::inverse(view.proj_view);
                view.stable_proj_view = view.projection * view.stable_view;
                view.inv_stable_proj_view = glm::inverse(view.stable_proj_view);
                view.eye_position = {};

                const auto frustum = make_perspective_frustum(view.proj_view);
                std::memcpy(view.frustum, frustum.data(), sizeof(view.frustum));
                view.resolution = glm::vec2(IRIS_VSM_VIRTUAL_BASE_SIZE);
                views[IRIS_SHADOW_VIEW_START + i] = view;
            }
        }
        view_buffer.insert(views);

        // write materials
        {
            material_buffer.insert(_scene.materials);
        }

        // write VSM info
        {
            vsm_globals_buffer.insert(_state.vsm_global_data);
        }
    }

    auto application_t::_resize() noexcept -> void {
        IR_PROFILE_SCOPED();
        _device->wait_idle();
        _swapchain.reset();
        _swapchain = ir::swapchain_t::make(*_device, *_wsi, {
            .vsync = false,
            .srgb = false,
        });
        _initialize_sync();
        _initialize_imgui();
        _frame_index = 0;
    }

    auto application_t::_resize_main_viewport() noexcept -> bool {
        _device->wait_idle();
        if (_state.dlss.is_initialized) {
            return false;
        }
        _state.dlss.render_resolution = {
            _gui.main_viewport_resolution.x * _state.dlss.scaling_ratio,
            _gui.main_viewport_resolution.y * _state.dlss.scaling_ratio,
        };
        IR_LOG_WARN(
            _device->logger(),
            "DLSS reinit | render resolution {}x{} | output resolution {}x{}",
            _state.dlss.render_resolution.x, _state.dlss.render_resolution.y,
            _gui.main_viewport_resolution.x, _gui.main_viewport_resolution.y);
        const auto width = _gui.main_viewport_resolution.x;
        const auto height = _gui.main_viewport_resolution.y;
        _initialize_sync();
        _initialize_dlss();
        _initialize_visbuffer_pass();
        _camera.update_aspect(width, height);
        _state.dlss.is_initialized = true;
        return true;
    }

    auto application_t::_initialize() noexcept -> void {
        IR_PROFILE_SCOPED();
        _wsi = ir::wsi_platform_t::make(1280, 720, "Test");
        _instance = ir::instance_t::make({
            .features = {
                .debug_markers = true,
            },
            .wsi_extensions = ir::wsi_platform_t::context_extensions(),
        });
        _device = ir::device_t::make(*_instance, {
            .features = {
                .swapchain = true,
                .mesh_shader = true,
                .image_atomics_64 = true,
                .dlss = true,
            }
        });
        _swapchain = ir::swapchain_t::make(*_device, *_wsi, {
            .vsync = false,
            .srgb = false,
        });
        _command_pools = ir::command_pool_t::make(*_device, frames_in_flight, {
            .queue = ir::queue_type_t::e_graphics,
            .flags = ir::command_pool_flag_t::e_transient,
        });
        for (const auto& pool : _command_pools) {
            _command_buffers.emplace_back(ir::command_buffer_t::make(*pool, {
                .primary = true,
            }));
        }

        _camera = camera_t::make(*_wsi);
        _camera.update(1 / 144.0f);

        _jitter_offsets = std::to_array<glm::vec2>({
            glm::vec2(0.500000f, 0.333333f) - 0.5f,
            glm::vec2(0.250000f, 0.666667f) - 0.5f,
            glm::vec2(0.750000f, 0.111111f) - 0.5f,
            glm::vec2(0.125000f, 0.444444f) - 0.5f,
            glm::vec2(0.625000f, 0.777778f) - 0.5f,
            glm::vec2(0.375000f, 0.222222f) - 0.5f,
            glm::vec2(0.875000f, 0.555556f) - 0.5f,
            glm::vec2(0.062500f, 0.888889f) - 0.5f,
            glm::vec2(0.562500f, 0.037037f) - 0.5f,
            glm::vec2(0.312500f, 0.370370f) - 0.5f,
            glm::vec2(0.812500f, 0.703704f) - 0.5f,
            glm::vec2(0.187500f, 0.148148f) - 0.5f,
            glm::vec2(0.687500f, 0.481481f) - 0.5f,
            glm::vec2(0.437500f, 0.814815f) - 0.5f,
            glm::vec2(0.937500f, 0.259259f) - 0.5f,
            glm::vec2(0.031250f, 0.592593f) - 0.5f,
        });
        _state.performance.frame_times.resize(512);
    }

    auto application_t::_initialize_dlss() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto render_width = _state.dlss.render_resolution.x;
        const auto render_height = _state.dlss.render_resolution.y;
        const auto output_width = _gui.main_viewport_resolution.x;
        const auto output_height = _gui.main_viewport_resolution.y;
        _device->ngx().initialize_dlss({
            .render_resolution = { render_width, render_height },
            .output_resolution = { output_width, output_height },
            .quality = _state.dlss.quality,
            .depth_scale = 1.0f,
            .is_hdr = true,
            .is_reverse_depth = true,
            .enable_auto_exposure = false,
        });
    }

    auto application_t::_initialize_imgui() noexcept -> void {
        IR_PROFILE_SCOPED();
        if (!_gui.is_initialized) {
            _gui.is_initialized = true;
            IMGUI_CHECKVERSION();
            _gui.main_pass = ir::render_pass_t::make(*_device, {
                .attachments = {
                    {
                        .layout = {
                            .final = ir::image_layout_t::e_transfer_src_optimal
                        },
                        .format = ir::resource_format_t::e_r8g8b8a8_unorm,
                        .load_op = ir::attachment_load_op_t::e_clear,
                        .store_op = ir::attachment_store_op_t::e_store,
                    }
                },
                .subpasses = {
                    {
                        .color_attachments = { 0 }
                    }
                },
                .dependencies = {
                    {
                        .source = ir::external_subpass,
                        .dest = 0,
                        .source_stage = ir::pipeline_stage_t::e_color_attachment_output,
                        .dest_stage = ir::pipeline_stage_t::e_color_attachment_output,
                        .source_access = ir::resource_access_t::e_none,
                        .dest_access = ir::resource_access_t::e_color_attachment_write,
                    }, {
                        .source = 0,
                        .dest = ir::external_subpass,
                        .source_stage = ir::pipeline_stage_t::e_color_attachment_output,
                        .dest_stage = ir::pipeline_stage_t::e_transfer,
                        .source_access = ir::resource_access_t::e_color_attachment_write,
                        .dest_access = ir::resource_access_t::e_transfer_read
                    }
                }
            });

            ImGui::CreateContext();
            ImPlot::CreateContext();
            auto& io = ImGui::GetIO();
            io.ConfigFlags =
                ImGuiConfigFlags_NavEnableKeyboard |
                ImGuiConfigFlags_DockingEnable;
            io.Fonts->AddFontFromFileTTF("../fonts/RobotoCondensed-Regular.ttf", 18);
            ImGui::StyleColorsDark();
            ImPlot::StyleColorsDark();
            {
                auto& style = ImGui::GetStyle();
                style.WindowPadding = ImVec2(12, 12);
                style.WindowRounding = 0.0f;
                style.FramePadding = ImVec2(4, 4);
                style.FrameRounding = 0.0f;
                style.ItemSpacing = ImVec2(8, 8);
                style.ItemInnerSpacing = ImVec2(4, 4);
                style.IndentSpacing = 16.0f;
                style.ScrollbarSize = 16.0f;
                style.ScrollbarRounding = 8.0f;
                style.GrabMinSize = 4.0f;
                style.GrabRounding = 3.0f;
            }
            ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(_wsi->window_handle()), true);
            auto imgui_vulkan_info = ImGui_ImplVulkan_InitInfo();
            imgui_vulkan_info.Instance = _instance->handle();
            imgui_vulkan_info.PhysicalDevice = _device->gpu();
            imgui_vulkan_info.Device = _device->handle();
            imgui_vulkan_info.QueueFamily = _device->graphics_queue().family();
            imgui_vulkan_info.Queue = _device->graphics_queue().handle();
            imgui_vulkan_info.PipelineCache = nullptr;
            imgui_vulkan_info.DescriptorPool = _device->descriptor_pool().handle();
            imgui_vulkan_info.Subpass = 0;
            imgui_vulkan_info.MinImageCount = 2;
            imgui_vulkan_info.ImageCount = _swapchain->images().size();
            imgui_vulkan_info.MSAASamples = ir::as_enum_counterpart(ir::sample_count_t::e_1);
            imgui_vulkan_info.Allocator = nullptr;
            imgui_vulkan_info.CheckVkResultFn = [](VkResult result) {
                if (result != VK_SUCCESS) {
                    spdlog::error("ImGui Vulkan error: {}", ir::as_string(result));
                }
            };
            ImGui_ImplVulkan_LoadFunctions([](const char* name, void* data) {
                return vkGetInstanceProcAddr((VkInstance)data, name);
            }, _instance->handle());
            ImGui_ImplVulkan_Init(&imgui_vulkan_info, _gui.main_pass->handle());
            _device->graphics_queue().submit([](ir::command_buffer_t& commands) {
                ImGui_ImplVulkan_CreateFontsTexture(commands.handle());
            });
            ImGui_ImplVulkan_DestroyFontUploadObjects();
        }
        _gui.main_image = ir::image_t::make_from_attachment(*_device, _gui.main_pass->attachment(0), {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .usage = ir::image_usage_t::e_color_attachment | ir::image_usage_t::e_transfer_src,
            .view = ir::default_image_view_info,
        });
        _gui.main_framebuffer = ir::framebuffer_t::make(*_gui.main_pass, {
            .attachments = { _gui.main_image },
            .width = _swapchain->width(),
            .height = _swapchain->height(),
        });
        _gui.main_layout = ir::descriptor_layout_t::make(*_device, std::to_array<ir::descriptor_binding_t>({
            {
                .set = 0,
                .binding = 0,
                .count = 1,
                .type = ir::descriptor_type_t::e_combined_image_sampler,
                .stage = ir::shader_stage_t::e_fragment,
            }
        }));
        _gui.main_sampler = ir::sampler_t::make(*_device, {
            .filter = { ir::sampler_filter_t::e_nearest },
            .mip_mode = ir::sampler_mipmap_mode_t::e_nearest,
            .address_mode = { ir::sampler_address_mode_t::e_repeat },
            .border_color = ir::sampler_border_color_t::e_float_opaque_black,
        });
    }

    auto application_t::_initialize_sync() noexcept -> void {
        IR_PROFILE_SCOPED();
        _image_available = ir::semaphore_t::make(*_device, frames_in_flight, {});
        _render_done = ir::semaphore_t::make(*_device, frames_in_flight, {});
        _frame_fence = ir::fence_t::make(*_device, frames_in_flight);
    }

    auto application_t::_initialize_visbuffer_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        if (!_visbuffer.is_initialized) {
            _visbuffer.is_initialized = true;
            _visbuffer.pass = ir::render_pass_t::make(*_device, {
                .attachments = {
                    {
                        .layout = {
                            .final = ir::image_layout_t::e_general
                        },
                        .format = ir::resource_format_t::e_r32_uint,
                        .load_op = ir::attachment_load_op_t::e_clear,
                        .store_op = ir::attachment_store_op_t::e_store,
                    },
                    {
                        .layout = {
                            .final = ir::image_layout_t::e_general
                        },
                        .format = ir::resource_format_t::e_r16g16_sfloat,
                        .load_op = ir::attachment_load_op_t::e_clear,
                        .store_op = ir::attachment_store_op_t::e_store,
                    },
                    {
                        .layout = {
                            .final = ir::image_layout_t::e_shader_read_only_optimal
                        },
                        .format = ir::resource_format_t::e_d32_sfloat,
                        .load_op = ir::attachment_load_op_t::e_clear,
                        .store_op = ir::attachment_store_op_t::e_store,
                    }
                },
                .subpasses = {
                    {
                        .color_attachments = { 0, 1 },
                        .depth_stencil_attachment = 2
                    }
                },
                .dependencies = {
                    {
                        .source = ir::external_subpass,
                        .dest = 0,
                        .source_stage =
                            ir::pipeline_stage_t::e_color_attachment_output |
                            ir::pipeline_stage_t::e_early_fragment_tests |
                            ir::pipeline_stage_t::e_late_fragment_tests,
                        .dest_stage =
                            ir::pipeline_stage_t::e_color_attachment_output |
                            ir::pipeline_stage_t::e_early_fragment_tests |
                            ir::pipeline_stage_t::e_late_fragment_tests,
                        .source_access = ir::resource_access_t::e_none,
                        .dest_access =
                            ir::resource_access_t::e_color_attachment_write |
                            ir::resource_access_t::e_depth_stencil_attachment_write,
                    }, {
                        .source = 0,
                        .dest = ir::external_subpass,
                        .source_stage =
                            ir::pipeline_stage_t::e_color_attachment_output |
                            ir::pipeline_stage_t::e_early_fragment_tests |
                            ir::pipeline_stage_t::e_late_fragment_tests,
                        .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                        .source_access =
                            ir::resource_access_t::e_color_attachment_write |
                            ir::resource_access_t::e_depth_stencil_attachment_write,
                        .dest_access = ir::resource_access_t::e_shader_read
                    }
                }
            });
            _visbuffer.main = ir::pipeline_t::make(*_device, *_visbuffer.pass, {
                .mesh = "../shaders/visbuffer/main.mesh.glsl",
                .fragment = "../shaders/visbuffer/main.frag.glsl",
                .blend = {
                    ir::attachment_blend_t::e_disabled,
                    ir::attachment_blend_t::e_disabled,
                },
                .dynamic_states = {
                    ir::dynamic_state_t::e_viewport,
                    ir::dynamic_state_t::e_scissor,
                },
                .depth_flags =
                    ir::depth_state_flag_t::e_enable_test |
                    ir::depth_state_flag_t::e_enable_write,
                .depth_compare_op = ir::compare_op_t::e_greater,
                .cull_mode = ir::cull_mode_t::e_back,
            });
            _visbuffer.resolve = ir::pipeline_t::make(*_device, {
                .compute = "../shaders/visbuffer/resolve.comp.glsl",
            });
            _visbuffer.tonemap = ir::pipeline_t::make(*_device, {
                .compute = "../shaders/visbuffer/tonemap.comp.glsl",
            });
        }
        const auto render_viewport_width = _state.dlss.render_resolution.x;
        const auto render_viewport_height = _state.dlss.render_resolution.y;
        const auto main_viewport_width = _gui.main_viewport_resolution.x;
        const auto main_viewport_height = _gui.main_viewport_resolution.y;
        _visbuffer.ids = ir::image_t::make_from_attachment(*_device, _visbuffer.pass->attachment(0), {
            .width = render_viewport_width,
            .height = render_viewport_height,
            .usage =
                ir::image_usage_t::e_color_attachment |
                ir::image_usage_t::e_storage,
            .view = ir::default_image_view_info,
        });
        _visbuffer.velocity = ir::image_t::make_from_attachment(*_device, _visbuffer.pass->attachment(1), {
            .width = render_viewport_width,
            .height = render_viewport_height,
            .usage =
                ir::image_usage_t::e_color_attachment |
                ir::image_usage_t::e_sampled,
            .view = ir::default_image_view_info,
        });
        _visbuffer.depth = ir::image_t::make_from_attachment(*_device, _visbuffer.pass->attachment(2), {
            .width = render_viewport_width,
            .height = render_viewport_height,
            .usage =
                ir::image_usage_t::e_depth_stencil_attachment |
                ir::image_usage_t::e_sampled,
            .view = ir::default_image_view_info,
        });
        _visbuffer.color = ir::image_t::make(*_device, {
            .width = render_viewport_width,
            .height = render_viewport_height,
            .usage =
                ir::image_usage_t::e_sampled |
                ir::image_usage_t::e_storage,
            .format = ir::resource_format_t::e_r32g32b32a32_sfloat,
            .view = ir::default_image_view_info,
        });
        _visbuffer.color_resolve = ir::image_t::make(*_device, {
            .width = main_viewport_width,
            .height = main_viewport_height,
            .usage =
                ir::image_usage_t::e_transfer_dst |
                ir::image_usage_t::e_sampled |
                ir::image_usage_t::e_storage,
            .format = ir::resource_format_t::e_r32g32b32a32_sfloat,
            .view = ir::default_image_view_info,
        });
        _visbuffer.final = ir::image_t::make(*_device, {
            .width = main_viewport_width,
            .height = main_viewport_height,
            .usage =
                ir::image_usage_t::e_sampled |
                ir::image_usage_t::e_storage,
            .format = ir::resource_format_t::e_r8g8b8a8_unorm,
            .view = ir::default_image_view_info,
        });
        _visbuffer.framebuffer = ir::framebuffer_t::make(*_visbuffer.pass, {
            .attachments = {
                _visbuffer.ids,
                _visbuffer.velocity,
                _visbuffer.depth,
            },
            .width = render_viewport_width,
            .height = render_viewport_height,
        });
        _visbuffer.prev_projection = _camera.projection();
        _visbuffer.prev_view = _camera.view();
    }

    auto application_t::_initialize_vsm_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        if (!_vsm.is_initialized) {
            _vsm.is_initialized = true;
            _vsm.visible_pages_buffer.device = ir::buffer_t<uint8>::make(*_device, {
                .usage =
                    ir::buffer_usage_t::e_storage_buffer |
                    ir::buffer_usage_t::e_transfer_src |
                    ir::buffer_usage_t::e_transfer_dst,
                .flags = ir::buffer_flag_t::e_resized,
                .capacity = IRIS_VSM_VIRTUAL_PAGE_COUNT * IRIS_VSM_MAX_CLIPMAPS,
            });
            _vsm.visible_pages_buffer.host = ir::buffer_t<uint8>::make(*_device, frames_in_flight, {
                .usage = ir::buffer_usage_t::e_transfer_dst,
                .memory = { ir::memory_property_t::e_host_cached },
                .flags =
                    ir::buffer_flag_t::e_resized |
                    ir::buffer_flag_t::e_mapped |
                    ir::buffer_flag_t::e_random_access,
                .capacity = IRIS_VSM_VIRTUAL_PAGE_COUNT * IRIS_VSM_MAX_CLIPMAPS,
            });
            _vsm.globals_buffer = ir::buffer_t<vsm_global_data_t>::make(*_device, frames_in_flight, {
                .usage = ir::buffer_usage_t::e_storage_buffer,
                .flags = ir::buffer_flag_t::e_mapped,
                .capacity = 1,
            });
            _vsm.hzb.image = ir::image_t::make(*_device, {
                .width = IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE,
                .height = IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE,
                .levels = 1 + glm::log2<uint32>(IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE),
                .layers = IRIS_VSM_CLIPMAP_COUNT,
                .usage =
                    ir::image_usage_t::e_storage |
                    ir::image_usage_t::e_sampled |
                    ir::image_usage_t::e_transfer_dst,
                .format = ir::resource_format_t::e_r8_uint,
                .view = ir::default_image_view_info,
            });
            for (auto i = 0_u32; i < IRIS_VSM_CLIPMAP_COUNT; ++i) {
                for (auto j = 0_u32; j < _vsm.hzb.image->levels(); ++j) {
                    _vsm.hzb.views.emplace_back(ir::image_view_t::make(*_vsm.hzb.image, {
                        .subresource = {
                            .level = j,
                            .level_count = 1,
                            .layer = i,
                            .layer_count = 1,
                        }
                    }));
                }
            }
            _vsm.mark_visible_pages = ir::pipeline_t::make(*_device, {
                .compute = "../shaders/vsm/mark_visible_pages.comp.glsl",
            });

            _device->graphics_queue().submit([this](ir::command_buffer_t& commands) {
                commands.image_barrier({
                    .image = std::cref(*_vsm.hzb.image),
                    .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
                    .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                    .source_access = ir::resource_access_t::e_none,
                    .dest_access = ir::resource_access_t::e_shader_storage_read,
                    .old_layout = ir::image_layout_t::e_undefined,
                    .new_layout = ir::image_layout_t::e_general,
                });
            });
        }
    }

    auto application_t::_clear_buffer_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        command_buffer.begin_debug_marker("clear_buffer_pass");
        command_buffer.buffer_barrier({
            .buffer = _buffer.atomics->slice(),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_transfer_write,
        });
        command_buffer.buffer_barrier({
            .buffer = _vsm.visible_pages_buffer.device->slice(),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_transfer_write,
        });
        command_buffer.fill_buffer(_buffer.atomics->slice(), 0);
        command_buffer.fill_buffer(_vsm.visible_pages_buffer.device->slice(), 0);
        command_buffer.buffer_barrier({
            .buffer = _buffer.atomics->slice(),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_shader_storage_write | ir::resource_access_t::e_shader_storage_read,
        });
        command_buffer.buffer_barrier({
            .buffer = _vsm.visible_pages_buffer.device->slice(),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_shader_storage_write | ir::resource_access_t::e_shader_storage_read,
        });
        command_buffer.end_debug_marker();
    }

    auto application_t::_visbuffer_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        const auto& [
            view_buffer,
            transform_buffer
        ] = _current_frame_buffers();

        command_buffer.begin_debug_marker("visbuffer_pass");
        command_buffer.begin_render_pass(*_visbuffer.framebuffer, {
            ir::make_clear_color({ -1_u32 }),
            ir::make_clear_color({ 0.0_f32 }),
            ir::make_clear_depth(0.0f, 0),
        });
        command_buffer.set_viewport({
            .width = static_cast<float32>(_visbuffer.color->width()),
            .height = static_cast<float32>(_visbuffer.color->height()),
        });
        command_buffer.set_scissor({
            .width = _visbuffer.color->width(),
            .height = _visbuffer.color->height(),
        });
        command_buffer.bind_pipeline(*_visbuffer.main);
        {
            const auto addresses = std::to_array({
                view_buffer.address(),
                _buffer.meshlet_instances->address(),
                _buffer.meshlets->address(),
                transform_buffer.address(),
                _buffer.vertices->address(),
                _buffer.indices->address(),
                _buffer.primitives->address(),
            });
            const auto jitter = _jitter_offsets[_device->frame_counter().current() % _jitter_offsets.size()];
            const auto push_constants = ir::make_byte_bag(addresses, jitter);
            command_buffer.push_constants(ir::shader_stage_t::e_mesh, 0, sizeof(push_constants), &push_constants);
        }
        command_buffer.draw_mesh_tasks(_buffer.meshlet_instances->size());
        command_buffer.end_render_pass();
        command_buffer.end_debug_marker();
    }

    auto application_t::_visbuffer_resolve_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        const auto& [
            view_buffer,
            transform_buffer
        ] = _current_frame_buffers();

        const auto& material_buffer = *_buffer.materials[_frame_index];
        const auto& directional_light_buffer = *_buffer.directional_lights[_frame_index];
        const auto& vsm_globals_buffer = *_vsm.globals_buffer[_frame_index];

        const auto set = ir::descriptor_set_builder_t(*_visbuffer.resolve, 0)
            .bind_sampled_image(0, _visbuffer.depth->view())
            .bind_storage_image(1, _visbuffer.ids->view())
            .bind_storage_image(2, _visbuffer.color->view())
            .bind_textures(3, _scene.textures)
            .build();
        const auto resolve_dispatch = dispatch_work_group_size({
            _visbuffer.color->width(),
            _visbuffer.color->height(),
        }, {
            16,
            16,
        });

        command_buffer.begin_debug_marker("visbuffer_resolve");
        command_buffer.bind_pipeline(*_visbuffer.resolve);
        command_buffer.bind_descriptor_set(*set);
        command_buffer.image_barrier({
            .image = std::cref(*_visbuffer.color),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_shader_storage_write,
            .old_layout = ir::image_layout_t::e_undefined,
            .new_layout = ir::image_layout_t::e_general,
        });

        {
            const auto push_constants = ir::make_byte_bag(std::to_array({
                view_buffer.address(),
                _buffer.meshlet_instances->address(),
                _buffer.meshlets->address(),
                transform_buffer.address(),
                _buffer.vertices->address(),
                _buffer.indices->address(),
                _buffer.primitives->address(),
                material_buffer.address(),
                directional_light_buffer.address(),
                vsm_globals_buffer.address(),
            }));
            command_buffer.push_constants(ir::shader_stage_t::e_compute, 0, sizeof(push_constants), &push_constants);
        }
        command_buffer.dispatch(resolve_dispatch.x, resolve_dispatch.y);
        command_buffer.image_barrier({
            .image = std::cref(*_visbuffer.color),
            .source_stage = ir::pipeline_stage_t::e_compute_shader,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_shader_storage_read,
            .old_layout = ir::image_layout_t::e_general,
            .new_layout = ir::image_layout_t::e_general,
        });
        command_buffer.end_debug_marker();
    }

    auto application_t::_visbuffer_dlss_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        const auto current_jitter = _jitter_offsets[_device->frame_counter().current() % _jitter_offsets.size()];

        command_buffer.begin_debug_marker("visbuffer_dlss_pass");
        command_buffer.image_barrier({
            .image = std::cref(*_visbuffer.color_resolve),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
            .dest_stage =
                ir::pipeline_stage_t::e_transfer |
                ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_none,
            .dest_access =
                ir::resource_access_t::e_shader_write |
                ir::resource_access_t::e_transfer_write,
            .old_layout = ir::image_layout_t::e_undefined,
            .new_layout = ir::image_layout_t::e_general,
        });
        _device->ngx().evaluate({
            .commands = std::ref(command_buffer),
            .color = std::ref(*_visbuffer.color),
            .depth = std::ref(*_visbuffer.depth),
            .velocity = std::ref(*_visbuffer.velocity),
            .output = std::ref(*_visbuffer.color_resolve),
            .jitter_offset = current_jitter,
            .reset = _state.dlss.reset,
        });
        command_buffer.image_barrier({
            .image = std::cref(*_visbuffer.color_resolve),
            .source_stage =
                ir::pipeline_stage_t::e_transfer |
                ir::pipeline_stage_t::e_compute_shader,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access =
                ir::resource_access_t::e_shader_write |
                ir::resource_access_t::e_transfer_write,
            .dest_access =  ir::resource_access_t::e_shader_storage_read,
            .old_layout = ir::image_layout_t::e_general,
            .new_layout = ir::image_layout_t::e_general,
        });

        command_buffer.end_debug_marker();
    }

    auto application_t::_visbuffer_tonemap_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        const auto set = ir::descriptor_set_builder_t(*_visbuffer.tonemap, 0)
            .bind_storage_image(0, _visbuffer.color_resolve->view())
            .bind_storage_image(1, _visbuffer.final->view())
            .build();
        const auto tonemap_dispatch = dispatch_work_group_size({
            _visbuffer.color_resolve->width(),
            _visbuffer.color_resolve->height(),
        }, {
            16,
            16,
        });

        command_buffer.begin_debug_marker("visbuffer_tonemap");
        command_buffer.bind_pipeline(*_visbuffer.tonemap);
        command_buffer.bind_descriptor_set(*set);
        command_buffer.image_barrier({
            .image = std::cref(*_visbuffer.final),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_shader_storage_write,
            .old_layout = ir::image_layout_t::e_undefined,
            .new_layout = ir::image_layout_t::e_general,
        });
        command_buffer.dispatch(tonemap_dispatch.x, tonemap_dispatch.y);
        command_buffer.image_barrier({
            .image = std::cref(*_visbuffer.final),
            .source_stage = ir::pipeline_stage_t::e_compute_shader,
            .dest_stage = ir::pipeline_stage_t::e_fragment_shader,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_shader_sampled_read,
            .old_layout = ir::image_layout_t::e_general,
            .new_layout = ir::image_layout_t::e_shader_read_only_optimal,
        });
        command_buffer.end_debug_marker();
    }

    auto application_t::_vsm_mark_visible_pages_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        const auto& [
            view_buffer,
            transform_buffer
        ] = _current_frame_buffers();

        const auto& vsm_globals_buffer = *_vsm.globals_buffer[_frame_index];
        const auto& vsm_visible_pages_buffer = *_vsm.visible_pages_buffer.host[_frame_index];

        const auto mark_page_dispatch = dispatch_work_group_size({
            _visbuffer.color->width(),
            _visbuffer.color->height(),
        }, {
            16,
            16,
        });

        const auto set = ir::descriptor_set_builder_t(*_vsm.mark_visible_pages, 0)
            .bind_sampled_image(0, _visbuffer.depth->view())
            .build();

        command_buffer.begin_debug_marker("vsm_mark_pages");
        command_buffer.bind_pipeline(*_vsm.mark_visible_pages);
        command_buffer.bind_descriptor_set(*set);
        command_buffer.buffer_barrier({
            .buffer = _vsm.visible_pages_buffer.device->slice(),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_shader_storage_write,
        });
        {
            const auto push_constants = ir::make_byte_bag(std::to_array({
                view_buffer.address(),
                vsm_globals_buffer.address(),
                _vsm.visible_pages_buffer.device->address(),
            }));
            command_buffer.push_constants(ir::shader_stage_t::e_compute, 0, sizeof(push_constants), &push_constants);
        }
        command_buffer.dispatch(mark_page_dispatch.x, mark_page_dispatch.y);
        command_buffer.buffer_barrier({
            .buffer = _vsm.visible_pages_buffer.device->slice(),
            .source_stage = ir::pipeline_stage_t::e_compute_shader,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_transfer_read,
        });
        for (auto i = 0_u32; i < IRIS_VSM_CLIPMAP_COUNT; ++i) {
            command_buffer.image_barrier({
                .image = std::cref(*_vsm.hzb.image),
                .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
                .dest_stage = ir::pipeline_stage_t::e_transfer,
                .source_access = ir::resource_access_t::e_none,
                .dest_access = ir::resource_access_t::e_transfer_write,
                .old_layout = ir::image_layout_t::e_undefined,
                .new_layout = ir::image_layout_t::e_transfer_dst_optimal,
                .subresource = {
                    .level = 0,
                    .level_count = 1,
                    .layer = i,
                    .layer_count = 1,
                }
            });
            command_buffer.copy_buffer_to_image(
                _vsm.visible_pages_buffer.device->slice(
                    i * IRIS_VSM_VIRTUAL_PAGE_COUNT,
                    IRIS_VSM_VIRTUAL_PAGE_COUNT
                ),
                *_vsm.hzb.image,
                {
                    .level = 0,
                    .level_count = 1,
                    .layer = i,
                    .layer_count = 1,
                }
            );
            command_buffer.image_barrier({
                .image = std::cref(*_vsm.hzb.image),
                .source_stage = ir::pipeline_stage_t::e_transfer,
                .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                .source_access = ir::resource_access_t::e_transfer_write,
                .dest_access =
                    ir::resource_access_t::e_shader_storage_read |
                    ir::resource_access_t::e_shader_storage_write,
                .old_layout = ir::image_layout_t::e_transfer_dst_optimal,
                .new_layout = ir::image_layout_t::e_general,
                .subresource = {
                    .level = 0,
                    .level_count = 1,
                    .layer = i,
                    .layer_count = 1,
                }
            });
        }
        command_buffer.buffer_barrier({
            .buffer = vsm_visible_pages_buffer.slice(),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_transfer_write,
        });
        command_buffer.copy_buffer(
            _vsm.visible_pages_buffer.device->slice(),
            vsm_visible_pages_buffer.slice(),
            {});
        command_buffer.buffer_barrier({
            .buffer = vsm_visible_pages_buffer.slice(),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_host,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_host_read,
        });
        command_buffer.buffer_barrier({
            .buffer =_vsm.visible_pages_buffer.device->slice(),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_shader_storage_write | ir::resource_access_t::e_shader_storage_read,
        });
        command_buffer.end_debug_marker();
    }

    auto application_t::_gui_main_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        {
            const auto* main_viewport = ImGui::GetMainViewport();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
            ImGui::SetNextWindowPos(main_viewport->Pos);
            ImGui::SetNextWindowSize(main_viewport->Size);
            ImGui::SetNextWindowViewport(main_viewport->ID);
            {
                const auto flags =
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                    ImGuiWindowFlags_NoNavFocus;
                if (ImGui::Begin("main_dock_space", nullptr, flags)) {
                    ImGui::DockSpace(ImGui::GetID("main_dock_space"));
                }
                ImGui::End();
            }
            ImGui::SetNextWindowSizeConstraints({ 64, 64 }, { 8192, 8192 });
            if (ImGui::Begin("Viewport")) {
                if (!ImGui::IsAnyMouseDown()) {
                    const auto resolution = ImGui::GetContentRegionAvail();
                    if (resolution.x != _gui.main_viewport_resolution.x || resolution.y != _gui.main_viewport_resolution.y) {
                        _state.dlss.is_initialized = false;
                    }
                    _gui.main_viewport_resolution = glm::vec2(
                        resolution.x,
                        resolution.y
                    );
                }
                const auto image_set = ir::descriptor_set_builder_t(*_gui.main_layout)
                    .bind_combined_image_sampler(0, _visbuffer.final->view(), *_gui.main_sampler)
                    .build();
                const auto size = ImVec2(
                    _gui.main_viewport_resolution.x,
                    _gui.main_viewport_resolution.y
                );
                ImGui::Image(image_set->handle(), size);
            }
            ImGui::End();
            ImGui::PopStyleVar(3);

            if (ImGui::Begin("Settings")) {
                if (ImGui::CollapsingHeader("DLSS Settings", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
                    constexpr static auto presets = std::to_array<const char*>({
                        "Performance",
                        "Balanced",
                        "Quality",
                        "DLAA"
                    });
                    const auto* current = presets[3];
                    switch (_state.dlss.quality) {
                        case ir::dlss_quality_preset_t::e_performance:
                            current = presets[0];
                            break;
                        case ir::dlss_quality_preset_t::e_balanced:
                            current = presets[1];
                            break;
                        case ir::dlss_quality_preset_t::e_quality:
                            current = presets[2];
                            break;
                        case ir::dlss_quality_preset_t::e_native:
                            current = presets[3];
                            break;
                    }
                    if (ImGui::BeginCombo("DLSS Quality Preset", current)) {
                        for (auto i = 0_u32; i < presets.size(); ++i) {
                            const auto is_selected = current == presets[i];
                            if (ImGui::Selectable(presets[i], is_selected)) {
                                current = presets[i];
                                switch (i) {
                                    case 0:
                                        _state.dlss.quality = ir::dlss_quality_preset_t::e_performance;
                                        _state.dlss.scaling_ratio = ir::dlss_preset_performance_scaling_ratio;
                                        break;
                                    case 1:
                                        _state.dlss.quality = ir::dlss_quality_preset_t::e_balanced;
                                        _state.dlss.scaling_ratio = ir::dlss_preset_balanced_scaling_ratio;
                                        break;
                                    case 2:
                                        _state.dlss.quality = ir::dlss_quality_preset_t::e_quality;
                                        _state.dlss.scaling_ratio = ir::dlss_preset_quality_scaling_ratio;
                                        break;
                                    case 3:
                                        _state.dlss.quality = ir::dlss_quality_preset_t::e_native;
                                        _state.dlss.scaling_ratio = ir::dlss_preset_native_scaling_ratio;
                                        break;
                                }
                                _state.dlss.is_initialized = false;
                            }
                            if (is_selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::Checkbox("Reset", &_state.dlss.reset);
                }
                ImGui::Separator();
            }
            ImGui::End();

            if (ImGui::Begin("Performance")) {
                ImGui::Text("FPS: %d - ms: %.4f", (uint32)(1.0f / _delta_time), _delta_time * 1000.0f);
                ImGui::Separator();
                if (ImPlot::BeginPlot("Frame Time", { -1, 0 }, ImPlotFlags_Crosshairs)) {
                    const auto axes_flags =
                        ImPlotAxisFlags_AutoFit |
                        ImPlotAxisFlags_RangeFit |
                        ImPlotAxisFlags_NoInitialFit;
                    ImPlot::SetupAxes("Frame", "ms", axes_flags, axes_flags);
                    auto time = std::vector<float32>(_state.performance.frame_times.size());
                    std::iota(time.begin(), time.end(), 0);
                    auto history = std::vector(_state.performance.frame_times.begin(), _state.performance.frame_times.end());
                    ImPlot::PlotLine("Delta Time", time.data(), history.data(), time.size());
                    ImPlot::EndPlot();
                }
            }
            ImGui::End();
        }
        ImGui::Render();
        command_buffer.begin_debug_marker("gui_draw");
        command_buffer.begin_render_pass(*_gui.main_framebuffer, {
            ir::make_clear_color({ 0.0f, 0.0f, 0.0f, 1.0f }),
        });
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer.handle());
        command_buffer.end_render_pass();
        command_buffer.end_debug_marker();
    }

    auto application_t::_swapchain_copy_pass(uint32 image_index) noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence
        ] = _current_frame_data();

        command_buffer.begin_debug_marker("copy_to_swapchain");
        command_buffer.image_barrier({
            .image = std::cref(_swapchain->image(image_index)),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_transfer_write,
            .old_layout = ir::image_layout_t::e_undefined,
            .new_layout = ir::image_layout_t::e_transfer_dst_optimal,
        });
        command_buffer.blit_image(*_gui.main_image, _swapchain->image(image_index), {});
        command_buffer.image_barrier({
            .image = std::cref(_swapchain->image(image_index)),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_bottom_of_pipe,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_none,
            .old_layout = ir::image_layout_t::e_transfer_dst_optimal,
            .new_layout = ir::image_layout_t::e_present_src,
        });
        command_buffer.end_debug_marker();
    }
}