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
        : _camera(_wsi) {
        IR_PROFILE_SCOPED();
        _initialize();
        _initialize_sync();
        _initialize_visbuffer_pass();
        _initialize_vsm_pass();

        _load_models();
    }

    application_t::~application_t() noexcept {
        IR_PROFILE_SCOPED();
        _device->wait_idle();
    }

    auto application_t::run() noexcept -> void {
        IR_PROFILE_SCOPED();
        while (!_wsi.should_close()) {
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
            *_frame_fence[_frame_index],
            *_sparse_fence[_frame_index]
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
            frame_fence,
            sparse_fence
        ] = _current_frame_data();

        auto [image_index, should_resize] = _swapchain->acquire_next_image(image_available);
        if (should_resize) {
            _resize();
            return;
        }

        command_buffer.pool().reset();
        command_buffer.begin();
        _clear_buffer_pass();
        _visbuffer_pass();
        _vsm_mark_visible_pages_pass();
        command_buffer.end();

        _device->graphics_queue().submit({
            .command_buffers = { std::cref(command_buffer) },
            .wait_semaphores = {},
            .signal_semaphores = {},
        }, &sparse_fence);
        sparse_fence.wait();
        sparse_fence.reset();
        _update_sparse_bindings();

        command_buffer.pool().reset();
        command_buffer.begin();
        _vsm_hardware_rasterize_pass();
        _visbuffer_resolve_pass();
        _visbuffer_tonemap_pass();
        _swapchain_copy_pass(image_index);
        command_buffer.end();

        auto sparse_timeline_value = 0_u64;
        auto graphics_wait_semaphores = std::vector<ir::queue_semaphore_stage_t> {
            { std::cref(image_available), ir::pipeline_stage_t::e_transfer }
        };
        auto graphics_signal_semaphores = std::vector<ir::queue_semaphore_stage_t> {
            { std::cref(render_done), ir::pipeline_stage_t::e_none }
        };
        if (!_vsm.sparse_binding_deferred_queue.empty()) {
            auto sparse_bind_info = ir::queue_bind_sparse_info_t();
            auto sparse_bind_count = 0_u32;
            while (sparse_bind_count < IRIS_MAX_SPARSE_BINDING_UPDATES && !_vsm.sparse_binding_deferred_queue.empty()) {
                const auto& current_sparse_bind_info = _vsm.sparse_binding_deferred_queue.front();
                if (current_sparse_bind_info.image_binds.empty()) {
                    _vsm.sparse_binding_deferred_queue.pop();
                    continue;
                }
                sparse_bind_info.image_binds.insert(
                    sparse_bind_info.image_binds.end(),
                    current_sparse_bind_info.image_binds.begin(),
                    current_sparse_bind_info.image_binds.end());
                for (const auto& binding : current_sparse_bind_info.image_binds) {
                    sparse_bind_count += binding.bindings.size();
                }
                _vsm.sparse_binding_deferred_queue.pop();
            }
            sparse_timeline_value = _sparse_bind_semaphore->increment(2);
            sparse_bind_info.wait_semaphores = {
                { std::cref(*_sparse_bind_semaphore), {}, sparse_timeline_value }
            };
            sparse_bind_info.signal_semaphores = {
                { std::cref(*_sparse_bind_semaphore), {}, sparse_timeline_value + 1 }
            };
            graphics_wait_semaphores.emplace_back(
                std::cref(*_sparse_bind_semaphore),
                ir::pipeline_stage_t::e_top_of_pipe,
                sparse_timeline_value + 1
            );
            graphics_signal_semaphores.emplace_back(
                std::cref(*_sparse_bind_semaphore),
                ir::pipeline_stage_t::e_bottom_of_pipe,
                sparse_timeline_value + 2
            );
            _device->compute_queue().bind_sparse(sparse_bind_info);
        } else {
            sparse_timeline_value = _sparse_bind_semaphore->increment();
            graphics_signal_semaphores.emplace_back(
                std::cref(*_sparse_bind_semaphore),
                ir::pipeline_stage_t::e_bottom_of_pipe,
                sparse_timeline_value + 1
            );
        }
        _device->graphics_queue().submit({
            .command_buffers = { std::cref(command_buffer) },
            .wait_semaphores = std::move(graphics_wait_semaphores),
            .signal_semaphores = std::move(graphics_signal_semaphores),
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
        auto [viewport_width, viewport_height] = _wsi.update_viewport();
        while (viewport_width == 0 || viewport_height == 0) {
            ir::wsi_platform_t::wait_events();
            std::tie(viewport_width, viewport_height) = _wsi.update_viewport();
        }
        _wsi.input().capture();
        if (_wsi.input().is_pressed_once(ir::mouse_t::e_right_button)) {
            _wsi.capture_cursor();
        }
        if (_wsi.input().is_released_once(ir::mouse_t::e_right_button)) {
            _wsi.release_cursor();
        }

        _camera.update(_delta_time);
        _device->tick();
    }

    auto application_t::_update_frame_data() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence,
            sparse_fence
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
            if (_wsi.input().is_pressed(ir::keyboard_t::e_up)) {
                elevation += 25.0f * _delta_time;
            }
            if (_wsi.input().is_pressed(ir::keyboard_t::e_down)) {
                elevation -= 25.0f * _delta_time;
            }
            if (_wsi.input().is_pressed(ir::keyboard_t::e_left)) {
                azimuth -= 25.0f * _delta_time;
            }
            if (_wsi.input().is_pressed(ir::keyboard_t::e_right)) {
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
            view.inv_projection = glm::inverse(view.projection);
            view.view = _camera.view();
            view.inv_view = glm::inverse(view.view);
            view.proj_view = view.projection * view.view;
            view.inv_proj_view = glm::inverse(view.proj_view);
            view.eye_position = glm::make_vec4(_camera.position());

            const auto frustum = make_perspective_frustum(view.proj_view);
            std::memcpy(view.frustum, frustum.data(), sizeof(view.frustum));
            view.resolution = glm::vec2(_swapchain->width(), _swapchain->height());
            views[IRIS_MAIN_VIEW_INDEX] = view;
        }

        // write shadow views
        {
            const auto& sun_dir_light = _state.directional_lights[0];
            for (auto i = 0_u32; i < IRIS_VSM_CLIPMAP_COUNT; ++i) {
                const auto width = _state.vsm_global_data.first_width * (1 << i) / 2.0f;

                auto view = view_t();
                view.projection = glm::ortho(-width, width, -width, width, -2000.0f, 2000.0f);
                view.projection[1][1] *= -1.0f;
                view.inv_projection = glm::inverse(view.projection);
                view.stable_view = glm::lookAt(sun_dir_light.direction, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                view.inv_stable_view = glm::inverse(view.stable_view);
                view.stable_proj_view = view.projection * view.stable_view;
                view.inv_stable_proj_view = glm::inverse(view.stable_proj_view);
                view.eye_position = {};

                // calculate view matrix based on main camera position shift to center
                {
                    const auto center = glm::trunc(_camera.position());
                    view.view = glm::lookAt(
                        center + sun_dir_light.direction,
                        center,
                        glm::vec3(0.0f, 1.0f, 0.0f));
                    view.inv_view = glm::inverse(view.view);
                    view.proj_view = view.projection * view.view;
                    view.inv_proj_view = glm::inverse(view.proj_view);
                }

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

    auto application_t::_update_sparse_bindings() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& vsm_visible_pages_buffer = *_vsm.visible_pages_buffer.host[_frame_index];
        auto current_sparse_bind_update_count = 0_u32;
        auto sparse_image_infos = std::vector<ir::sparse_image_memory_bind_info_t>();
        auto sparse_image_bindings = std::vector<ir::sparse_image_memory_bind_t>();
        for (auto i = 0_u32; i < IRIS_VSM_CLIPMAP_COUNT; ++i) {
            const auto requests = vsm_visible_pages_buffer.as_span();
            const auto bindings = _vsm.images[i].make_updated_sparse_bindings(requests.subspan(
                i * IRIS_VSM_VIRTUAL_PAGE_COUNT,
                IRIS_VSM_VIRTUAL_PAGE_COUNT
            ));
            auto sparse_image_memory_binds = std::vector<ir::sparse_image_memory_bind_t>();
            sparse_image_bindings.insert(sparse_image_bindings.end(), bindings.begin(), bindings.end());
            if (!sparse_image_bindings.empty()) {
                current_sparse_bind_update_count += sparse_image_bindings.size();
                sparse_image_infos.emplace_back(ir::sparse_image_memory_bind_info_t {
                    .image = std::cref(_vsm.images[i].image()),
                    .bindings = std::move(sparse_image_bindings),
                });
                if (current_sparse_bind_update_count > IRIS_MAX_SPARSE_BINDING_UPDATES) {
                    _vsm.sparse_binding_deferred_queue.push(ir::queue_bind_sparse_info_t {
                        .image_binds = { std::move(sparse_image_infos) },
                    });
                    sparse_image_infos.clear();
                    current_sparse_bind_update_count = 0;
                }
                sparse_image_bindings.clear();
            }
        }
        if (!sparse_image_infos.empty()) {
            _vsm.sparse_binding_deferred_queue.push(ir::queue_bind_sparse_info_t {
                .image_binds = { std::move(sparse_image_infos) },
            });
        }
    }

    auto application_t::_resize() noexcept -> void {
        IR_PROFILE_SCOPED();
        _device->wait_idle();
        _swapchain.reset();
        _swapchain = ir::swapchain_t::make(*_device, _wsi, {
            .vsync = false,
            .srgb = false,
        });
        _initialize_sync();
        _initialize_visbuffer_pass();
        _frame_index = 0;
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
            }
        });
        _swapchain = ir::swapchain_t::make(*_device, _wsi, {
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

        _camera = camera_t::make(_wsi);
        _camera.update(1 / 144.0f);
    }

    auto application_t::_initialize_sync() noexcept -> void {
        IR_PROFILE_SCOPED();
        _image_available = ir::semaphore_t::make(*_device, frames_in_flight, {});
        _render_done = ir::semaphore_t::make(*_device, frames_in_flight, {});
        _frame_fence = ir::fence_t::make(*_device, frames_in_flight);
        _sparse_fence = ir::fence_t::make(*_device, frames_in_flight, false);
        _sparse_bind_semaphore = ir::semaphore_t::make(*_device, {
            .counter = 0,
            .timeline = true,
        });
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
                            .final = ir::image_layout_t::e_shader_read_only_optimal
                        },
                        .format = ir::resource_format_t::e_d32_sfloat,
                        .load_op = ir::attachment_load_op_t::e_clear,
                        .store_op = ir::attachment_store_op_t::e_store,
                    }
                },
                .subpasses = {
                    {
                        .color_attachments = { 0 },
                        .depth_stencil_attachment = 1
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
                    ir::attachment_blend_t::e_disabled
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
        _visbuffer.ids = ir::image_t::make_from_attachment(*_device, _visbuffer.pass->attachment(0), {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .usage = ir::image_usage_t::e_color_attachment | ir::image_usage_t::e_storage,
            .view = ir::default_image_view_info,
        });
        _visbuffer.depth = ir::image_t::make_from_attachment(*_device, _visbuffer.pass->attachment(1), {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .usage = ir::image_usage_t::e_depth_stencil_attachment | ir::image_usage_t::e_sampled,
            .view = ir::default_image_view_info,
        });
        _visbuffer.color = ir::image_t::make(*_device, {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .usage = ir::image_usage_t::e_storage,
            .format = ir::resource_format_t::e_r32g32b32a32_sfloat,
            .view = ir::default_image_view_info,
        });
        _visbuffer.final = ir::image_t::make(*_device, {
            .width = _swapchain->width(),
            .height = _swapchain->height(),
            .usage = ir::image_usage_t::e_storage | ir::image_usage_t::e_transfer_src,
            .format = ir::resource_format_t::e_r8g8b8a8_unorm,
            .view = ir::default_image_view_info,
        });
        _visbuffer.framebuffer = ir::framebuffer_t::make(*_visbuffer.pass, {
            .attachments = {
                _visbuffer.ids,
                _visbuffer.depth,
            },
            .width = _swapchain->width(),
            .height = _swapchain->height(),
        });
    }

    auto application_t::_initialize_vsm_pass() noexcept -> void {
        if (!_vsm.is_initialized) {
            _vsm.is_initialized = true;
            _vsm.memory = ir::buffer_t<float32>::make(*_device, {
                .memory = { ir::memory_property_t::e_device_local },
                .capacity = IRIS_VSM_PHYSICAL_BASE_RESOLUTION
            });
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
            _vsm.images.reserve(IRIS_VSM_CLIPMAP_COUNT);
            _vsm.image_views.reserve(IRIS_VSM_CLIPMAP_COUNT);
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
                _vsm.images.emplace_back(virtual_shadow_map_t::make(*_device, _vsm.allocator, *_vsm.memory));
                _vsm.image_views.emplace_back(std::cref(_vsm.images.back().image().view()));
            }
            _vsm.sampler = ir::sampler_t::make(*_device, {
                .filter = { ir::sampler_filter_t::e_linear },
                .mip_mode = ir::sampler_mipmap_mode_t::e_nearest,
                .address_mode = { ir::sampler_address_mode_t::e_repeat },
                .border_color = ir::sampler_border_color_t::e_float_opaque_white,
                .compare = ir::compare_op_t::e_less,
            });
            _vsm.rasterize_pass = ir::render_pass_t::make(*_device, {
                .attachments = {
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
                        .depth_stencil_attachment = 0
                    }
                },
                .dependencies = {
                    {
                        .source = ir::external_subpass,
                        .dest = 0,
                        .source_stage =
                            ir::pipeline_stage_t::e_early_fragment_tests |
                            ir::pipeline_stage_t::e_late_fragment_tests,
                        .dest_stage =
                            ir::pipeline_stage_t::e_early_fragment_tests |
                            ir::pipeline_stage_t::e_late_fragment_tests,
                        .source_access = ir::resource_access_t::e_none,
                        .dest_access = ir::resource_access_t::e_depth_stencil_attachment_write,
                    }, {
                        .source = 0,
                        .dest = ir::external_subpass,
                        .source_stage =
                            ir::pipeline_stage_t::e_early_fragment_tests |
                            ir::pipeline_stage_t::e_late_fragment_tests,
                        .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                        .source_access = ir::resource_access_t::e_depth_stencil_attachment_write,
                        .dest_access = ir::resource_access_t::e_shader_sampled_read
                    }
                }
            });
            _vsm.rasterize_framebuffers.reserve(IRIS_VSM_CLIPMAP_COUNT);
            for (auto i = 0_u32; i < IRIS_VSM_CLIPMAP_COUNT; ++i) {
                const auto& image = _vsm.images[i].image();
                _vsm.rasterize_framebuffers.emplace_back(ir::framebuffer_t::make(*_vsm.rasterize_pass, {
                    .attachments = {
                        image.as_intrusive_ptr(),
                    },
                    .width = image.width(),
                    .height = image.height(),
                }));
            }
            _vsm.rasterize_hardware = ir::pipeline_t::make(*_device, *_vsm.rasterize_pass, {
                .mesh = "../shaders/vsm/rasterize_hardware.mesh.glsl",
                .fragment = "../shaders/vsm/rasterize_hardware.frag.glsl",
                .blend = {},
                .dynamic_states = {
                    ir::dynamic_state_t::e_viewport,
                    ir::dynamic_state_t::e_scissor,
                },
                .depth_flags =
                    ir::depth_state_flag_t::e_enable_test |
                    ir::depth_state_flag_t::e_enable_write |
                    ir::depth_state_flag_t::e_enable_clamp,
                .cull_mode = ir::cull_mode_t::e_front,
            });
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
            frame_fence,
            sparse_fence
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
            frame_fence,
            sparse_fence
        ] = _current_frame_data();

        const auto& [
            view_buffer,
            transform_buffer
        ] = _current_frame_buffers();

        command_buffer.begin_debug_marker("visbuffer_pass");
        command_buffer.begin_render_pass(*_visbuffer.framebuffer, {
            ir::make_clear_color({ -1_u32 }),
            ir::make_clear_depth(0.0f, 0),
        });
        command_buffer.set_viewport({
            .width = static_cast<float32>(_swapchain->width()),
            .height = static_cast<float32>(_swapchain->height()),
        });
        command_buffer.set_scissor({
            .width = _swapchain->width(),
            .height = _swapchain->height(),
        });
        command_buffer.bind_pipeline(*_visbuffer.main);
        {
            const auto push_constants = ir::make_byte_bag(std::to_array({
                view_buffer.address(),
                _buffer.meshlet_instances->address(),
                _buffer.meshlets->address(),
                transform_buffer.address(),
                _buffer.vertices->address(),
                _buffer.indices->address(),
                _buffer.primitives->address(),
            }));
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
            frame_fence,
            sparse_fence
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
            .bind_combined_image_sampler(3, _vsm.image_views, *_vsm.sampler)
            .bind_textures(4, _scene.textures)
            .build();
        const auto resolve_dispatch = dispatch_work_group_size({
            _swapchain->width(),
            _swapchain->height(),
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

    auto application_t::_visbuffer_tonemap_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence,
            sparse_fence
        ] = _current_frame_data();

        const auto set = ir::descriptor_set_builder_t(*_visbuffer.tonemap, 0)
            .bind_storage_image(0, _visbuffer.color->view())
            .bind_storage_image(1, _visbuffer.final->view())
            .build();
        const auto tonemap_dispatch = dispatch_work_group_size({
            _swapchain->width(),
            _swapchain->height(),
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
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_transfer_read,
            .old_layout = ir::image_layout_t::e_general,
            .new_layout = ir::image_layout_t::e_transfer_src_optimal,
        });
        command_buffer.end_debug_marker();
    }

    auto application_t::_vsm_mark_visible_pages_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence,
            sparse_fence
        ] = _current_frame_data();

        const auto& [
            view_buffer,
            transform_buffer
        ] = _current_frame_buffers();

        const auto& vsm_globals_buffer = *_vsm.globals_buffer[_frame_index];
        const auto& vsm_visible_pages_buffer = *_vsm.visible_pages_buffer.host[_frame_index];

        const auto mark_page_dispatch = dispatch_work_group_size({
            _swapchain->width(),
            _swapchain->height(),
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

    auto application_t::_vsm_hardware_rasterize_pass() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence,
            sparse_fence
        ] = _current_frame_data();

        const auto& [
            view_buffer,
            transform_buffer
        ] = _current_frame_buffers();

        command_buffer.begin_debug_marker("vsm_hardware_rasterize_pass");
        for (auto i = 0_u32; i < IRIS_VSM_CLIPMAP_COUNT; ++i) {
            command_buffer.begin_debug_marker("vsm_hardware_rasterize_clipmap_pass");
            if (_vsm.images[i].is_empty()) {
                command_buffer.image_barrier({
                    .image = std::cref(_vsm.images[i].image()),
                    .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
                    .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                    .source_access = ir::resource_access_t::e_none,
                    .dest_access = ir::resource_access_t::e_shader_sampled_read,
                    .old_layout = ir::image_layout_t::e_undefined,
                    .new_layout = ir::image_layout_t::e_shader_read_only_optimal,
                });
                command_buffer.end_debug_marker();
                continue;
            }
            const auto& current = _vsm.images[i].image();
            command_buffer.begin_render_pass(*_vsm.rasterize_framebuffers[i], {
                ir::make_clear_depth(1.0f, 0),
            });
            command_buffer.set_viewport({
                .width = static_cast<float32>(current.width()),
                .height = static_cast<float32>(current.height()),
            });
            command_buffer.set_scissor({
                .width = current.width(),
                .height = current.height(),
            });
            command_buffer.bind_pipeline(*_vsm.rasterize_hardware);
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
                const auto push_constants = ir::make_byte_bag(addresses, i);
                command_buffer.push_constants(ir::shader_stage_t::e_mesh, 0, sizeof(push_constants), &push_constants);
            }
            command_buffer.draw_mesh_tasks(_buffer.meshlet_instances->size());
            command_buffer.end_render_pass();
            command_buffer.end_debug_marker();
        }
        command_buffer.end_debug_marker();
    }

    auto application_t::_swapchain_copy_pass(uint32 image_index) noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& [
            command_buffer,
            image_available,
            render_done,
            frame_fence,
            sparse_fence
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
        command_buffer.blit_image(*_visbuffer.final, _swapchain->image(image_index), {});
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