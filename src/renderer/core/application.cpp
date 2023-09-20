#include <iris/core/enums.hpp>

#include <iris/gfx/device.hpp>
#include <iris/gfx/instance.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/command_pool.hpp>
#include <iris/gfx/sampler.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/framebuffer.hpp>
#include <iris/gfx/pipeline.hpp>
#include <iris/gfx/fence.hpp>
#include <iris/gfx/buffer.hpp>
#include <iris/gfx/sparse_page_allocator.hpp>
#include <iris/gfx/texture.hpp>
#include <iris/gfx/descriptor_set.hpp>
#include <iris/gfx/descriptor_pool.hpp>
#include <iris/gfx/descriptor_layout.hpp>
#include <iris/gfx/clear_value.hpp>
#include <iris/gfx/semaphore.hpp>
#include <iris/gfx/queue.hpp>
#include <iris/gfx/swapchain.hpp>
#include <iris/gfx/render_pass.hpp>

#include <renderer/core/application.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define VSM_TILE_SIZE_X 128
#define VSM_TILE_SIZE_Y 128
#define VSM_RESOLUTION 16384
#define VSM_PAGE_COUNT (16384 / VSM_TILE_SIZE_X)

namespace app {
    static auto previous_power_2(uint32 x) noexcept {
        auto r = 1_u32;
        while ((r << 1) < x) {
            r <<= 1;
        }
        return r;
    }

    application_t::application_t() noexcept
        : _camera(_platform) {
        IR_PROFILE_SCOPED();
        _platform = ir::wsi_platform_t::make(1920, 1080, "Iris");
        _instance = ir::instance_t::make({
            .features = {
                .debug_markers = true,
            },
            .wsi_extensions = ir::wsi_platform_t::context_extensions()
        });
        _device = ir::device_t::make(*_instance, {
            .features = {
                .swapchain = true,
                .mesh_shader = true,
                .image_atomics_64 = true,
                .fragment_shading_rate = true
            }
        });
        _swapchain = ir::swapchain_t::make(*_device, _platform, {
            .vsync = false
        });
        _main_pass.description = ir::render_pass_t::make(*_device, {
            .attachments = {},
            .subpasses = { ir::subpass_info_t() },
            .dependencies = {}
        });
        _main_pass.framebuffer = ir::framebuffer_t::make(*_main_pass.description, {
            .width = swapchain().width(),
            .height = swapchain().height(),
            .layers = 1
        });
        _main_pass.visbuffer = ir::image_t::make(*_device, {
            .width = swapchain().width(),
            .height = swapchain().height(),
            .usage = ir::image_usage_t::e_storage | ir::image_usage_t::e_transfer_dst,
            .format = ir::resource_format_t::e_r64_uint,
            .view = ir::default_image_view_info,
        });
        _main_pass.pipeline = ir::pipeline_t::make(*_device, *_main_pass.framebuffer, {
            .mesh = "../shaders/0.1/main.mesh.glsl",
            .fragment = "../shaders/0.1/main.frag",
            .dynamic_states = {
                ir::dynamic_state_t::e_viewport,
                ir::dynamic_state_t::e_scissor
            },
            .cull_mode = ir::cull_mode_t::e_back,
        });
        _main_pass.camera_buffer = ir::buffer_t<camera_data_t>::make(*_device, frames_in_flight, {
            .usage = ir::buffer_usage_t::e_uniform_buffer,
            .flags = ir::buffer_flag_t::e_mapped,
            .capacity = 1,
        });

        const auto hiz_width = previous_power_2(swapchain().width());
        const auto hiz_height = previous_power_2(swapchain().height());
        const auto hiz_mips = static_cast<uint32>(1 + glm::floor(glm::log2(static_cast<float32>(glm::max(hiz_width, hiz_height)))));
        _hiz_pass.depth = ir::image_t::make(*_device, {
            .width = hiz_width,
            .height = hiz_height,
            .levels = hiz_mips,
            .usage = ir::image_usage_t::e_storage | ir::image_usage_t::e_sampled,
            .format = ir::resource_format_t::e_r32_sfloat,
            .view = ir::default_image_view_info,
        });
        for (auto i = 0_u32; i < hiz_mips; ++i) {
            _hiz_pass.views.emplace_back(ir::image_view_t::make(*_hiz_pass.depth, {
                .subresource {
                    .level = i,
                    .level_count = 1,
                }
            }));
        }
        _hiz_pass.sampler = ir::sampler_t::make(*_device, {
            .filter = { ir::sampler_filter_t::e_linear },
            .mip_mode = ir::sampler_mipmap_mode_t::e_nearest,
            .address_mode = { ir::sampler_address_mode_t::e_clamp_to_edge },
            .reduction_mode = ir::sampler_reduction_mode_t::e_min,
        });
        _hiz_pass.copy = ir::pipeline_t::make(*_device, {
            .compute = "../shaders/0.1/hiz_copy.comp",
        });
        _hiz_pass.reduce = ir::pipeline_t::make(*_device, {
            .compute = "../shaders/0.1/hiz_reduce.comp",
        });

        _final_pass.description = ir::render_pass_t::make(*_device, {
            .attachments = {
                {
                    .layout = {
                        .final = ir::image_layout_t::e_transfer_src_optimal
                    },
                    .format = swapchain().format(),
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
                    .dest_access = ir::resource_access_t::e_color_attachment_write
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
        _final_pass.color = ir::image_t::make_from_attachment(*_device, _final_pass.description->attachment(0), {
            .width = swapchain().width(),
            .height = swapchain().height(),
            .usage =
                ir::image_usage_t::e_color_attachment |
                ir::image_usage_t::e_transfer_src,
            .view = ir::default_image_view_info,
        });
        _final_pass.framebuffer = ir::framebuffer_t::make(*_final_pass.description, {
            .attachments = {
                _final_pass.color
            },
            .width = swapchain().width(),
            .height = swapchain().height(),
            .layers = 1,
        });
        _final_pass.clear_values = {
            ir::make_clear_color({ 0.597f, 0.808f, 1.0f, 1.0f })
        };
        _final_pass.pipeline = ir::pipeline_t::make(*_device, *_final_pass.framebuffer, {
            .vertex = "../shaders/0.1/final.vert",
            .fragment = "../shaders/0.1/final.frag",
            .blend = {
                ir::attachment_blend_t::e_disabled
            },
            .dynamic_states = {
                ir::dynamic_state_t::e_viewport,
                ir::dynamic_state_t::e_scissor
            },
        });

        {
            const auto model = meshlet_model_t::make("../models/compressed/reactor/reactor.glb");
            auto meshlets = std::vector<meshlet_glsl_t>();
            meshlets.reserve(model.meshlet_count());
            for (const auto& meshlet : model.meshlets()) {
                meshlets.emplace_back(meshlet_glsl_t {
                    .vertex_offset = meshlet.vertex_offset,
                    .index_offset = meshlet.index_offset,
                    .primitive_offset = meshlet.primitive_offset,
                    .index_count = meshlet.index_count,
                    .primitive_count = meshlet.primitive_count,
                    .aabb = meshlet.aabb,
                    .material = {
                        .base_color_texture = meshlet.material.base_color_texture,
                        .normal_texture = meshlet.material.normal_texture
                    }
                });
            }

            for (const auto textures = model.textures(); const auto& [_, type, file] : textures) {
                _main_pass.textures.emplace_back(ir::texture_t::make(*_device, file, {
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

            _main_pass.meshlets = ir::upload_buffer<meshlet_glsl_t>(*_device, meshlets, {});
            _main_pass.meshlet_instances = ir::upload_buffer(*_device, model.meshlet_instances(), {});
            _main_pass.vertices = ir::upload_buffer(*_device, model.vertices(), {});
            _main_pass.indices = ir::upload_buffer(*_device, model.indices(), {});
            _main_pass.primitives = ir::upload_buffer(*_device, model.primitives(), {});

            auto transforms = std::vector<glm::mat4>(model.transforms().begin(), model.transforms().end());
            for (auto& transform : transforms) {
                //transform = glm::scale(transform, glm::vec3(0.1f));
            }
            _main_pass.transforms = ir::upload_buffer<glm::mat4>(*_device, transforms, {});
        }
        _main_pass.atomics = ir::buffer_t<uint32>::make(*_device, {
            .usage = ir::buffer_usage_t::e_storage_buffer | ir::buffer_usage_t::e_transfer_dst,
            .flags = ir::buffer_flag_t::e_resized,
            .capacity = 1024,
        });

        _cluster_pass.pipeline = ir::pipeline_t::make(*_device, {
            .compute = "../shaders/0.1/cull_classify.comp",
        });
        _cluster_pass.sw_rast = ir::buffer_t<uint32>::make(*_device, {
            .usage = ir::buffer_usage_t::e_storage_buffer | ir::buffer_usage_t::e_transfer_dst,
            .flags = ir::buffer_flag_t::e_resized,
            .capacity = _main_pass.meshlet_instances->size() + 1,
        });
        _cluster_pass.hw_rast = ir::buffer_t<uint32>::make(*_device, {
            .usage = ir::buffer_usage_t::e_storage_buffer | ir::buffer_usage_t::e_transfer_dst,
            .flags = ir::buffer_flag_t::e_resized,
            .capacity = _main_pass.meshlet_instances->size() + 1,
        });
        _cluster_pass.sw_command = ir::buffer_t<glm::uvec3>::make(*_device, {
            .usage = ir::buffer_usage_t::e_storage_buffer | ir::buffer_usage_t::e_indirect_buffer,
            .flags = ir::buffer_flag_t::e_resized,
            .capacity = 1,
        });
        _cluster_pass.hw_command = ir::buffer_t<glm::uvec3>::make(*_device, {
            .usage = ir::buffer_usage_t::e_storage_buffer | ir::buffer_usage_t::e_indirect_buffer,
            .flags = ir::buffer_flag_t::e_resized,
            .capacity = 1,
        });

        _shadow_pass.map = ir::image_t::make(*_device, {
            .width = 16384,
            .height = 16384,
            .usage = ir::image_usage_t::e_storage | ir::image_usage_t::e_sampled,
            .flags = ir::image_flag_t::e_sparse_binding | ir::image_flag_t::e_sparse_residency,
            .format = ir::resource_format_t::e_r32_sfloat,
            .view = ir::default_image_view_info,
        });
        _shadow_pass.analyze = ir::pipeline_t::make(*_device, {
            .compute = "../shaders/0.1/shadow_page_request.comp",
        });
        _shadow_pass.hzb = ir::image_t::make(*_device, {
            .width = VSM_TILE_SIZE_X,
            .height = VSM_TILE_SIZE_Y,
            .levels = static_cast<uint32>(1 + glm::floor(glm::log2(static_cast<float32>(VSM_TILE_SIZE_X)))),
            .usage =
                ir::image_usage_t::e_storage |
                ir::image_usage_t::e_sampled |
                ir::image_usage_t::e_transfer_dst,
            .format = ir::resource_format_t::e_r8_uint,
            .view = ir::default_image_view_info,
        });
        _shadow_pass.hzb_views.reserve(_shadow_pass.hzb->levels());
        for (auto i = 0_u32; i < _shadow_pass.hzb->levels(); ++i) {
            _shadow_pass.hzb_views.emplace_back(ir::image_view_t::make(*_shadow_pass.hzb, {
                .subresource {
                    .level = i,
                    .level_count = 1,
                }
            }));
        }
        _shadow_pass.reduce = ir::pipeline_t::make(*_device, {
            .compute = "../shaders/0.1/shadow_page_reduce.comp",
        });
        _shadow_pass.camera = ir::buffer_t<glm::mat4>::make(*_device, frames_in_flight, {
            .usage = ir::buffer_usage_t::e_storage_buffer,
            .flags = ir::buffer_flag_t::e_mapped,
            .capacity = 1,
        });
        _shadow_pass.device_requests = ir::buffer_t<uint8>::make(*_device, {
            .usage =
                ir::buffer_usage_t::e_storage_buffer |
                ir::buffer_usage_t::e_transfer_src |
                ir::buffer_usage_t::e_transfer_dst,
            .flags = ir::buffer_flag_t::e_resized,
            .capacity = VSM_TILE_SIZE_X * VSM_TILE_SIZE_Y,
        });
        _shadow_pass.host_requests = ir::buffer_t<uint8>::make(*_device, frames_in_flight, {
            .usage = ir::buffer_usage_t::e_storage_buffer | ir::buffer_usage_t::e_transfer_dst,
            .flags =
                ir::buffer_flag_t::e_mapped |
                ir::buffer_flag_t::e_random_access |
                ir::buffer_flag_t::e_resized,
            .capacity = VSM_TILE_SIZE_X * VSM_TILE_SIZE_Y,
        });

        _compute_rast_pass.pipeline = ir::pipeline_t::make(*_device, {
            .compute = "../shaders/0.1/rasterizer.comp",
        });

        _command_pools = ir::command_pool_t::make(*_device, frames_in_flight, {
            .queue = ir::queue_type_t::e_graphics,
            .flags = ir::command_pool_flag_t::e_transient
        });
        for (auto& pool : _command_pools) {
            _command_buffers.emplace_back(ir::command_buffer_t::make(*pool, {}));
        }
        _sparse_done = ir::semaphore_t::make(*_device, { 0, true });
        _image_available = ir::semaphore_t::make(*_device, frames_in_flight, {});
        _render_done = ir::semaphore_t::make(*_device, frames_in_flight, {});
        _frame_fence = ir::fence_t::make(*_device, frames_in_flight, true);

        // initial transitions
        device().graphics_queue().submit([this](ir::command_buffer_t& cmd) {
            IR_PROFILE_SCOPED();
            cmd.image_barrier({
                .image = std::cref(*_hiz_pass.depth),
                .source_stage = ir::pipeline_stage_t::e_none,
                .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                .source_access = ir::resource_access_t::e_none,
                .dest_access = ir::resource_access_t::e_shader_read,
                .old_layout = ir::image_layout_t::e_undefined,
                .new_layout = ir::image_layout_t::e_shader_read_only_optimal,
            });
            cmd.image_barrier({
                .image = std::cref(*_shadow_pass.hzb),
                .source_stage = ir::pipeline_stage_t::e_none,
                .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                .source_access = ir::resource_access_t::e_none,
                .dest_access = ir::resource_access_t::e_shader_read,
                .old_layout = ir::image_layout_t::e_undefined,
                .new_layout = ir::image_layout_t::e_shader_read_only_optimal,
            });
        });
    }

    application_t::~application_t() noexcept {
        device().wait_idle();
    }

    auto application_t::run() noexcept -> void {
        IR_PROFILE_SCOPED();
        while (!_platform.should_close()) {
            _update();
            _render();
            _frame_index = (_frame_index + 1) % frames_in_flight;
            IR_MARK_FRAME();
        }
    }

    auto application_t::wsi_platform() noexcept -> ir::wsi_platform_t& {
        IR_PROFILE_SCOPED();
        return _platform;
    }

    auto application_t::device() noexcept -> ir::device_t& {
        IR_PROFILE_SCOPED();
        return *_device;
    }

    auto application_t::swapchain() noexcept -> ir::swapchain_t& {
        IR_PROFILE_SCOPED();
        return *_swapchain;
    }

    auto application_t::command_pool(uint32 frame_index) noexcept -> ir::command_pool_t& {
        IR_PROFILE_SCOPED();
        return *_command_pools[frame_index == -1_u32 ? _frame_index : frame_index];
    }

    auto application_t::command_buffer(uint32 frame_index) noexcept -> ir::command_buffer_t& {
        IR_PROFILE_SCOPED();
        return *_command_buffers[frame_index == -1_u32 ? _frame_index : frame_index];
    }

    auto application_t::image_available(uint32 frame_index) noexcept -> ir::semaphore_t& {
        IR_PROFILE_SCOPED();
        return *_image_available[frame_index == -1_u32 ? _frame_index : frame_index];
    }

    auto application_t::render_done(uint32 frame_index) noexcept -> ir::semaphore_t& {
        IR_PROFILE_SCOPED();
        return *_render_done[frame_index == -1_u32 ? _frame_index : frame_index];
    }

    auto application_t::frame_fence(uint32 frame_index) noexcept -> ir::fence_t& {
        IR_PROFILE_SCOPED();
        return *_frame_fence[frame_index == -1_u32 ? _frame_index : frame_index];
    }

    auto application_t::camera_buffer(uint32 frame_index) noexcept -> ir::buffer_t<camera_data_t>& {
        IR_PROFILE_SCOPED();
        return *_main_pass.camera_buffer[frame_index == -1_u32 ? _frame_index : frame_index];
    }

    auto application_t::_update() noexcept -> void {
        IR_PROFILE_SCOPED();
        ir::wsi_platform_t::poll_events();

        const auto current_time = ch::steady_clock::now();
        _delta_time = ch::duration<float32>(current_time - _last_time).count();
        _last_time = current_time;
        device().tick();
        wsi_platform().input().capture();
        if (wsi_platform().input().is_pressed_once(ir::mouse_t::e_right_button)) {
            wsi_platform().capture_cursor();
        }
        if (wsi_platform().input().is_released_once(ir::mouse_t::e_right_button)) {
            wsi_platform().release_cursor();
        }
        _camera.update(_delta_time);

        if (wsi_platform().input().is_pressed_once(ir::keyboard_t::e_f1)) {
            _state.view_mode = 0;
        }
        if (wsi_platform().input().is_pressed_once(ir::keyboard_t::e_f2)) {
            _state.view_mode = 1;
        }
        if (wsi_platform().input().is_pressed_once(ir::keyboard_t::e_f3)) {
            _state.view_mode = 2;
        }
        if (wsi_platform().input().is_pressed_once(ir::keyboard_t::e_f4)) {
            _state.enable_hw = true;
            _state.enable_sw = false;
        }
        if (wsi_platform().input().is_pressed_once(ir::keyboard_t::e_f5)) {
            _state.enable_hw = false;
            _state.enable_sw = true;
        }
        if (wsi_platform().input().is_pressed_once(ir::keyboard_t::e_f6)) {
            _state.enable_hw = true;
            _state.enable_sw = true;
        }
    }

    auto application_t::_render() noexcept -> void {
        IR_PROFILE_SCOPED();

        frame_fence().wait();
        frame_fence().reset();
        command_buffer().pool().reset();
        const auto image_index = swapchain().acquire_next_image(image_available());

        static auto freeze_frustum = false;
        static auto last_camera = camera_data_t();

        if (wsi_platform().input().is_pressed_once(ir::keyboard_t::e_f)) {
            freeze_frustum = !freeze_frustum;
        }

        auto camera_data = camera_data_t {
            .projection = _camera.projection(),
            .view = _camera.view(),
            .old_pv = _camera.projection() * _camera.view(),
            .pv = _camera.projection() * _camera.view(),
            .position = glm::make_vec4(_camera.position()),
            .frustum = make_perspective_frustum(_camera.projection() * _camera.view())
        };

        last_camera.pv = _camera.projection() * _camera.view();
        last_camera.position = glm::make_vec4(_camera.position());
        if (!freeze_frustum) {
            last_camera = camera_data;
        }

        static auto sparse_allocator = ir::sparse_page_allocator_t::make(*_device, *_shadow_pass.map);
        const auto shadow_pages = sparse_allocator.request_pages({
            _shadow_pass.host_requests[_frame_index]->data(),
            _shadow_pass.host_requests[_frame_index]->size()
        });
        if (device().frame_counter().current() % 60 == 0) {
            IR_LOG_INFO(device().logger(), "shadow binding updates: {}", shadow_pages.size());
        }
        auto sparse_semaphore_value = 0_u64;
        static auto __sparse_once = false;
        if (!shadow_pages.empty() && !__sparse_once) {
            __sparse_once = true;
            sparse_semaphore_value = _sparse_done->increment(2);
            device().graphics_queue().wait_idle();
            device().graphics_queue().bind_sparse({
                .wait_semaphores = {
                    { std::cref(*_sparse_done), {}, sparse_semaphore_value }
                },
                .signal_semaphores = {
                    { std::cref(*_sparse_done), {}, sparse_semaphore_value + 1 }
                },
                .sparse_image_bindings = shadow_pages,
            });
        }

        camera_buffer().insert(last_camera);
        _shadow_pass.camera[_frame_index]->insert(
            glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, 0.001f, 200.0f) *
            glm::lookAt(glm::normalize(glm::vec3(25.0f, 100.0f, 25.0f)), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f)));

        auto main_set = ir::descriptor_set_builder_t(*_main_pass.pipeline, 0)
            .bind_uniform_buffer(0, camera_buffer().slice())
            .bind_storage_image(1, _main_pass.visbuffer->view())
            .build();

        auto cluster_area_set = ir::descriptor_set_builder_t(*_cluster_pass.pipeline, 0)
            .bind_uniform_buffer(0, camera_buffer().slice())
            .bind_combined_image_sampler(
                1,
                _hiz_pass.depth->view(),
                *_hiz_pass.sampler)
            .build();

        auto shadow_page_req_set = ir::descriptor_set_builder_t(*_shadow_pass.analyze, 0)
            .bind_uniform_buffer(0, camera_buffer().slice())
            .bind_storage_image(1, _main_pass.visbuffer->view())
            .build();

        const auto meshlet_count = _main_pass.meshlet_instances->size();

        command_buffer().begin();
        command_buffer().begin_debug_marker("clear_buffers");
        command_buffer().fill_buffer(_main_pass.atomics->slice(), 0);
        command_buffer().fill_buffer(_cluster_pass.sw_rast->slice(), 0);
        command_buffer().fill_buffer(_cluster_pass.hw_rast->slice(), 0);
        command_buffer().fill_buffer(_shadow_pass.device_requests->slice(), 0);
        command_buffer().buffer_barrier({
            .buffer = _shadow_pass.device_requests->slice(),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_shader_storage_write
        });
        command_buffer().buffer_barrier({
            .buffer = _main_pass.atomics->slice(),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access =
                ir::resource_access_t::e_shader_storage_read |
                ir::resource_access_t::e_shader_storage_write
        });
        command_buffer().buffer_barrier({
            .buffer = _cluster_pass.sw_rast->slice(),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access =
                ir::resource_access_t::e_shader_storage_read |
                ir::resource_access_t::e_shader_storage_write
        });
        command_buffer().buffer_barrier({
            .buffer = _cluster_pass.hw_rast->slice(),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access =
                ir::resource_access_t::e_shader_storage_read |
                ir::resource_access_t::e_shader_storage_write
        });
        command_buffer().end_debug_marker();

        command_buffer().begin_debug_marker("cull_and_classify");
        command_buffer().bind_pipeline(*_cluster_pass.pipeline);
        command_buffer().bind_descriptor_set(*cluster_area_set);
        {
#pragma pack(push, 1)
            struct {
                uint64 meshlet_ptr;
                uint64 meshlet_instances_ptr;
                uint64 transform_ptr;
                uint64 sw_meshlet_offset_ptr;
                uint64 hw_meshlet_offset_ptr;
                uint64 sw_command_ptr;
                uint64 hw_command_ptr;
                uint64 global_atomics_ptr;
                uint32 meshlet_count;
                uint32 width;
                uint32 height;
            } constants = {
                _main_pass.meshlets->address(),
                _main_pass.meshlet_instances->address(),
                _main_pass.transforms->address(),
                _cluster_pass.sw_rast->address(),
                _cluster_pass.hw_rast->address(),
                _cluster_pass.sw_command->address(),
                _cluster_pass.hw_command->address(),
                _main_pass.atomics->address(),
                static_cast<uint32>(meshlet_count),
                swapchain().width(),
                swapchain().height()
            };
#pragma pack(pop)
            command_buffer().push_constants(ir::shader_stage_t::e_compute, 0, ir::size_bytes(constants), &constants);
        }
        command_buffer().dispatch((meshlet_count + 255) / 256);
        command_buffer().buffer_barrier({
            .buffer = _cluster_pass.sw_rast->slice(),
            .source_stage = ir::pipeline_stage_t::e_compute_shader,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_shader_storage_read
        });
        command_buffer().buffer_barrier({
            .buffer = _cluster_pass.hw_rast->slice(),
            .source_stage = ir::pipeline_stage_t::e_compute_shader,
            .dest_stage = ir::pipeline_stage_t::e_mesh_shader,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_shader_storage_read
        });
        command_buffer().buffer_barrier({
            .buffer = _cluster_pass.sw_command->slice(),
            .source_stage = ir::pipeline_stage_t::e_compute_shader,
            .dest_stage = ir::pipeline_stage_t::e_draw_indirect,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_indirect_command_read
        });
        command_buffer().buffer_barrier({
            .buffer = _cluster_pass.hw_command->slice(),
            .source_stage = ir::pipeline_stage_t::e_compute_shader,
            .dest_stage = ir::pipeline_stage_t::e_draw_indirect,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_indirect_command_read
        });
        command_buffer().end_debug_marker();

        command_buffer().begin_debug_marker("hardware_rasterizer");
        command_buffer().image_barrier({
            .image = std::cref(*_main_pass.visbuffer),
            .source_stage = ir::pipeline_stage_t::e_none,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_transfer_write,
            .old_layout = ir::image_layout_t::e_undefined,
            .new_layout = ir::image_layout_t::e_transfer_dst_optimal,
        });
        command_buffer().clear_image(*_main_pass.visbuffer, ir::make_clear_color({ 0_u32 }), {});
        command_buffer().image_barrier({
            .image = std::cref(*_main_pass.visbuffer),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage =
                ir::pipeline_stage_t::e_fragment_shader |
                ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access =
                ir::resource_access_t::e_shader_storage_write |
                ir::resource_access_t::e_shader_storage_read,
            .old_layout = ir::image_layout_t::e_transfer_dst_optimal,
            .new_layout = ir::image_layout_t::e_general,
        });
        if (_state.enable_hw) {
            command_buffer().begin_render_pass(*_main_pass.framebuffer, {});
            command_buffer().bind_pipeline(*_main_pass.pipeline);
            command_buffer().set_viewport({
                .width = static_cast<float32>(swapchain().width()),
                .height = static_cast<float32>(swapchain().height()),
            }, true);
            command_buffer().set_scissor({
                .width = swapchain().width(),
                .height = swapchain().height(),
            });
            command_buffer().bind_descriptor_set(*main_set);
            {
                struct {
                    uint64 meshlet_ptr;
                    uint64 instance_ptr;
                    uint64 vertex_ptr;
                    uint64 index_ptr;
                    uint64 primitive_ptr;
                    uint64 transform_ptr;
                    uint64 hw_meshlet_offset_ptr;
                } constants = {
                    _main_pass.meshlets->address(),
                    _main_pass.meshlet_instances->address(),
                    _main_pass.vertices->address(),
                    _main_pass.indices->address(),
                    _main_pass.primitives->address(),
                    _main_pass.transforms->address(),
                    _cluster_pass.hw_rast->address(),
                };
                command_buffer().push_constants(ir::shader_stage_t::e_mesh, 0, ir::size_bytes(constants), &constants);
            }
            command_buffer().draw_mesh_tasks_indirect(_cluster_pass.hw_command->slice(), 1);
            command_buffer().end_render_pass();
            command_buffer().image_barrier({
                .image = std::cref(*_main_pass.visbuffer),
                .source_stage = ir::pipeline_stage_t::e_fragment_shader,
                .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                .source_access = ir::resource_access_t::e_shader_storage_write,
                .dest_access =
                    ir::resource_access_t::e_shader_storage_read |
                    ir::resource_access_t::e_shader_storage_write,
                .old_layout = ir::image_layout_t::e_general,
                .new_layout = ir::image_layout_t::e_general,
            });
        }
        command_buffer().end_debug_marker();

        if (_state.enable_sw) {
            auto compute_raster_set = ir::descriptor_set_builder_t(*_compute_rast_pass.pipeline, 0)
                .bind_uniform_buffer(0, camera_buffer().slice())
                .bind_storage_image(1, _main_pass.visbuffer->view())
                .build();
            command_buffer().begin_debug_marker("compute_rasterizer");
            command_buffer().bind_pipeline(*_compute_rast_pass.pipeline);
            command_buffer().bind_descriptor_set(*compute_raster_set);
            {
                struct {
                    uint64 meshlet_address;
                    uint64 meshlet_instance_address;
                    uint64 vertex_address;
                    uint64 index_address;
                    uint64 primitive_address;
                    uint64 transform_address;
                    uint64 sw_rast_address;
                } constants = {
                    _main_pass.meshlets->address(),
                    _main_pass.meshlet_instances->address(),
                    _main_pass.vertices->address(),
                    _main_pass.indices->address(),
                    _main_pass.primitives->address(),
                    _main_pass.transforms->address(),
                    _cluster_pass.sw_rast->address(),
                };
                command_buffer().push_constants(ir::shader_stage_t::e_compute, 0, ir::size_bytes(constants), &constants);
            }
            command_buffer().dispatch_indirect(_cluster_pass.sw_command->slice());
            command_buffer().end_debug_marker();
        }

        auto final_set = ir::descriptor_set_builder_t(*_final_pass.pipeline, 0)
            .bind_uniform_buffer(0, camera_buffer().slice())
            .bind_storage_image(1, _main_pass.visbuffer->view())
            .bind_textures(2, _main_pass.textures)
            .build();

        command_buffer().begin_debug_marker("rasterize_final");
        command_buffer().image_barrier({
            .image = std::cref(*_main_pass.visbuffer),
            .source_stage =
                ir::pipeline_stage_t::e_compute_shader |
                ir::pipeline_stage_t::e_fragment_shader,
            .dest_stage =
                ir::pipeline_stage_t::e_compute_shader |
                ir::pipeline_stage_t::e_fragment_shader,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_shader_storage_read,
            .old_layout = ir::image_layout_t::e_general,
            .new_layout = ir::image_layout_t::e_general,
        });
        command_buffer().begin_render_pass(*_final_pass.framebuffer, _final_pass.clear_values);
        command_buffer().bind_pipeline(*_final_pass.pipeline);
        command_buffer().set_viewport({
            .width = static_cast<float32>(swapchain().width()),
            .height = static_cast<float32>(swapchain().height()),
        }, true);
        command_buffer().set_scissor({
            .width = swapchain().width(),
            .height = swapchain().height(),
        });
        command_buffer().bind_descriptor_set(*final_set);
        {
#pragma pack(push, 1)
            struct {
                uint64 meshlet_address;
                uint64 meshlet_instance_address;
                uint64 vertex_address;
                uint64 index_address;
                uint64 primitive_address;
                uint64 transform_address;
                uint32 view_mode;
            } constants = {
                _main_pass.meshlets->address(),
                _main_pass.meshlet_instances->address(),
                _main_pass.vertices->address(),
                _main_pass.indices->address(),
                _main_pass.primitives->address(),
                _main_pass.transforms->address(),
                _state.view_mode,
            };
#pragma pack(pop)
            command_buffer().push_constants(ir::shader_stage_t::e_fragment, 0, ir::size_bytes(constants), &constants);
        }
        command_buffer().draw(3, 1, 0, 0);
        command_buffer().end_render_pass();
        command_buffer().end_debug_marker();

        command_buffer().begin_debug_marker("shadow_request_page");
        command_buffer().bind_pipeline(*_shadow_pass.analyze);
        command_buffer().bind_descriptor_set(*shadow_page_req_set);
        {
            struct {
                uint64 shadow_camera_address;
                uint64 shadow_page_request_address;
            } constants = {
                _shadow_pass.camera[_frame_index]->address(),
                _shadow_pass.device_requests->address(),
            };
            command_buffer().push_constants(ir::shader_stage_t::e_compute, 0, ir::size_bytes(constants), &constants);
        }
        command_buffer().dispatch(
            (_main_pass.visbuffer->width() + 15) / 16,
            (_main_pass.visbuffer->height() + 15) / 16);
        command_buffer().buffer_barrier({
            .buffer = _shadow_pass.device_requests->slice(),
            .source_stage = ir::pipeline_stage_t::e_compute_shader,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_transfer_read,
        });
        command_buffer().end_debug_marker();

        command_buffer().begin_debug_marker("hiz_build");
        if (!freeze_frustum) {
            {
                auto depth_copy_set = ir::descriptor_set_builder_t(*_hiz_pass.copy, 0)
                    .bind_storage_image(0, _main_pass.visbuffer->view())
                    .bind_storage_image(1, *_hiz_pass.views[0])
                    .build();
                command_buffer().image_barrier({
                    .image = std::cref(*_hiz_pass.depth),
                    .source_stage = ir::pipeline_stage_t::e_compute_shader,
                    .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                    .source_access = ir::resource_access_t::e_none,
                    .dest_access = ir::resource_access_t::e_shader_storage_write,
                    .old_layout = ir::image_layout_t::e_shader_read_only_optimal,
                    .new_layout = ir::image_layout_t::e_general,
                });
                command_buffer().bind_pipeline(*_hiz_pass.copy);
                command_buffer().bind_descriptor_set(*depth_copy_set);
                command_buffer().dispatch(
                    (_hiz_pass.depth->width() + 15) / 16,
                    (_hiz_pass.depth->height() + 15) / 16);
            }

            for (auto i = 0_u32; i < _hiz_pass.views.size() - 1; ++i) {
                auto depth_reduce_set = ir::descriptor_set_builder_t(*_hiz_pass.reduce, 0)
                    .bind_combined_image_sampler(0, *_hiz_pass.views[i], *_hiz_pass.sampler)
                    .bind_storage_image(1, *_hiz_pass.views[i + 1])
                    .build();
                command_buffer().image_barrier({
                    .image = std::cref(*_hiz_pass.depth),
                    .source_stage = ir::pipeline_stage_t::e_compute_shader,
                    .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                    .source_access = ir::resource_access_t::e_shader_storage_write,
                    .dest_access = ir::resource_access_t::e_shader_read,
                    .old_layout = ir::image_layout_t::e_general,
                    .new_layout = ir::image_layout_t::e_shader_read_only_optimal,
                    .subresource = {
                        .level = i,
                        .level_count = 1
                    },
                });
                const auto width = std::max(1_u32, _hiz_pass.depth->width() >> (i + 1));
                const auto height = std::max(1_u32, _hiz_pass.depth->height() >> (i + 1));
                command_buffer().bind_pipeline(*_hiz_pass.reduce);
                command_buffer().bind_descriptor_set(*depth_reduce_set);
                command_buffer().push_constants(ir::shader_stage_t::e_compute, 0, sizeof(glm::uvec2), glm::value_ptr(glm::uvec2(width, height)));
                command_buffer().dispatch(
                    (width + 15) / 16,
                    (height + 15) / 16);
            }
            command_buffer().image_barrier({
                .image = std::cref(*_hiz_pass.depth),
                .source_stage = ir::pipeline_stage_t::e_compute_shader,
                .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                .source_access = ir::resource_access_t::e_shader_storage_write,
                .dest_access = ir::resource_access_t::e_shader_read,
                .old_layout = ir::image_layout_t::e_general,
                .new_layout = ir::image_layout_t::e_shader_read_only_optimal,
                .subresource = {
                    .level = static_cast<uint32>(_hiz_pass.views.size() - 1),
                    .level_count = 1
                },
            });
        }
        command_buffer().end_debug_marker();

        command_buffer().begin_debug_marker("copy_to_swapchain");
        command_buffer().image_barrier({
            .image = std::cref(swapchain().image(image_index)),
            .source_stage = ir::pipeline_stage_t::e_none,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_transfer_write,
            .old_layout = ir::image_layout_t::e_undefined,
            .new_layout = ir::image_layout_t::e_transfer_dst_optimal,
        });
        command_buffer().copy_image(
            std::cref(*_final_pass.color),
            std::cref(swapchain().image(image_index)),
            {});
        command_buffer().image_barrier({
            .image = std::cref(swapchain().image(image_index)),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_none,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_none,
            .old_layout = ir::image_layout_t::e_transfer_dst_optimal,
            .new_layout = ir::image_layout_t::e_present_src,
        });
        command_buffer().end_debug_marker();

        command_buffer().begin_debug_marker("transfer_buffers");
        command_buffer().image_barrier({
            .image = std::cref(*_shadow_pass.hzb),
            .source_stage = ir::pipeline_stage_t::e_none,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_transfer_write,
            .old_layout = ir::image_layout_t::e_undefined,
            .new_layout = ir::image_layout_t::e_transfer_dst_optimal,
        });
        command_buffer().copy_buffer_to_image(
            _shadow_pass.device_requests->slice(),
            *_shadow_pass.hzb,
            {
                .level = 0,
                .level_count = 1
            });
        command_buffer().image_barrier({
            .image = std::cref(*_shadow_pass.hzb),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_compute_shader,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access =
                ir::resource_access_t::e_shader_storage_read |
                ir::resource_access_t::e_shader_storage_write,
            .old_layout = ir::image_layout_t::e_transfer_dst_optimal,
            .new_layout = ir::image_layout_t::e_general,
        });
        // reduce page hzb
        for (auto i = 1_u32; i < _shadow_pass.hzb->levels(); ++i) {
            auto hzb_reduce_set = ir::descriptor_set_builder_t(*_shadow_pass.reduce, 0)
                .bind_storage_image(0, *_shadow_pass.hzb_views[i - 1])
                .bind_storage_image(1, *_shadow_pass.hzb_views[i])
                .build();
            const auto width = std::max(1_u32, _shadow_pass.hzb->width() >> i);
            const auto height = std::max(1_u32, _shadow_pass.hzb->height() >> i);
            command_buffer().bind_pipeline(*_shadow_pass.reduce);
            command_buffer().bind_descriptor_set(*hzb_reduce_set);
            command_buffer().dispatch(
                (width + 15) / 16,
                (height + 15) / 16);
            command_buffer().image_barrier({
                .image = std::cref(*_shadow_pass.hzb),
                .source_stage = ir::pipeline_stage_t::e_compute_shader,
                .dest_stage = ir::pipeline_stage_t::e_compute_shader,
                .source_access = ir::resource_access_t::e_shader_storage_write,
                .dest_access = ir::resource_access_t::e_shader_read,
                .old_layout = ir::image_layout_t::e_general,
                .new_layout = ir::image_layout_t::e_general,
                .subresource = {
                    .level = i,
                    .level_count = 1
                },
            });
        }
        command_buffer().copy_buffer(
            _shadow_pass.device_requests->slice(),
            _shadow_pass.host_requests[_frame_index]->slice(),
            {});
        command_buffer().buffer_barrier({
            .buffer = _shadow_pass.host_requests[_frame_index]->slice(),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_host,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_host_read,
        });

        command_buffer().end_debug_marker();
        command_buffer().end();
        {
            auto waits = std::vector<ir::queue_semaphore_stage_t> {
                { std::cref(image_available()), ir::pipeline_stage_t::e_transfer }
            };
            auto signals = std::vector<ir::queue_semaphore_stage_t> {
                { std::cref(render_done()), ir::pipeline_stage_t::e_none }
            };
            if (!shadow_pages.empty()) {
                waits.push_back({
                    std::cref(*_sparse_done),
                    ir::pipeline_stage_t::e_top_of_pipe,
                    sparse_semaphore_value + 1
                });
                signals.push_back({
                    std::cref(*_sparse_done),
                    ir::pipeline_stage_t::e_bottom_of_pipe,
                    sparse_semaphore_value + 2
                });
            } else {
                signals.push_back({
                    std::cref(*_sparse_done),
                    ir::pipeline_stage_t::e_bottom_of_pipe,
                    _sparse_done->increment(1) + 1
                });
            }
            device().graphics_queue().submit({
                .command_buffers = { std::cref(command_buffer()) },
                .wait_semaphores = waits,
                .signal_semaphores = signals,
            }, &frame_fence());
        }
        device().graphics_queue().present({
            .swapchain = std::cref(swapchain()),
            .wait_semaphores = {
                std::cref(render_done())
            },
            .image = image_index
        });
    }
}
