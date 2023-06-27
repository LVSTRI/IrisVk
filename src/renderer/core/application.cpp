#include <iris/gfx/device.hpp>
#include <iris/gfx/instance.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/command_pool.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/framebuffer.hpp>
#include <iris/gfx/pipeline.hpp>
#include <iris/gfx/fence.hpp>
#include <iris/gfx/buffer.hpp>
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

namespace app {
    constexpr static auto task_workgroup_size = 32;

    application_t::application_t() noexcept
        : _camera(_platform) {
        IR_PROFILE_SCOPED();
        _platform = ir::wsi_platform_t::make(1920, 1080, "Iris");
        _instance = ir::instance_t::make({
            .features = {
                .debug_markers = true
            },
            .wsi_extensions = ir::wsi_platform_t::context_extensions()
        });
        _device = ir::device_t::make(*_instance, {
            .features = {
                .swapchain = true,
                .mesh_shader = true
            }
        });
        _swapchain = ir::swapchain_t::make(*_device, _platform, {
            .vsync = true
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
            .task = "../shaders/0.1/main.task.glsl",
            .mesh = "../shaders/0.1/main.mesh.glsl",
            .fragment = "../shaders/0.1/main.frag",
            .blend = {
                ir::attachment_blend_t::e_disabled
            },
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
                    .source_stage =
                        ir::pipeline_stage_t::e_color_attachment_output,
                    .dest_stage =
                        ir::pipeline_stage_t::e_color_attachment_output,
                    .source_access = ir::resource_access_t::e_none,
                    .dest_access =
                        ir::resource_access_t::e_color_attachment_write
                }, {
                    .source = 0,
                    .dest = ir::external_subpass,
                    .source_stage =
                        ir::pipeline_stage_t::e_color_attachment_output,
                    .dest_stage = ir::pipeline_stage_t::e_transfer,
                    .source_access =
                        ir::resource_access_t::e_color_attachment_write,
                    .dest_access = ir::resource_access_t::e_transfer_read
                }
            }
        });
        _final_pass.color = ir::image_t::make_from_attachment(*_device, _final_pass.description.as_const_ref().attachment(0), {
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
            const auto model = meshlet_model_t::make("../models/compressed/trees/trees.glb");
            auto meshlets = std::vector<meshlet_glsl_t>();
            meshlets.reserve(model.meshlet_count());
            for (auto group_id = 0_u32; const auto& group : model.meshlet_groups()) {
                for (const auto& meshlet : group.meshlets) {
                    meshlets.emplace_back(meshlet_glsl_t {
                        .vertex_offset = group.vertex_offset,
                        .index_offset = meshlet.index_offset,
                        .primitive_offset = meshlet.primitive_offset,
                        .index_count = meshlet.index_count,
                        .primitive_count = meshlet.primitive_count,
                        .instance_id = group.instance_id,
                        .aabb = meshlet.aabb
                    });
                }
                group_id++;
            }

            _main_pass.meshlets = ir::upload_buffer(*_device, std::span<const meshlet_glsl_t>(meshlets), {});
            _main_pass.vertices = ir::upload_buffer(*_device, model.vertices(), {});
            _main_pass.indices = ir::upload_buffer(*_device, model.indices(), {});
            _main_pass.primitives = ir::upload_buffer(*_device, model.primitives(), {});
            _main_pass.transforms = ir::upload_buffer(*_device, model.transforms(), {});
        }

        _command_pools = ir::command_pool_t::make(*_device, frames_in_flight, {
            .queue = ir::queue_type_t::e_graphics,
            .flags = ir::command_pool_flag_t::e_transient
        });
        for (auto& pool : _command_pools) {
            _command_buffers.emplace_back(ir::command_buffer_t::make(*pool, {}));
        }
        _image_available = ir::semaphore_t::make(*_device, frames_in_flight);
        _render_done = ir::semaphore_t::make(*_device, frames_in_flight);
        _frame_fence = ir::fence_t::make(*_device, frames_in_flight);
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
    }

    auto application_t::_render() noexcept -> void {
        IR_PROFILE_SCOPED();

        frame_fence().wait();
        frame_fence().reset();
        command_buffer().pool().reset();
        const auto image_index = swapchain().acquire_next_image(image_available());

        static auto freeze_frustum = false;
        static auto last_frustum = frustum_t();

        if (wsi_platform().input().is_pressed_once(ir::keyboard_t::e_f)) {
            freeze_frustum = !freeze_frustum;
        }

        if (!freeze_frustum) {
            last_frustum = make_perspective_frustum(_camera.projection() * _camera.view());
        }

        auto camera_data = camera_data_t {
            .projection = _camera.projection(),
            .view = _camera.view(),
            .pv = _camera.projection() * _camera.view(),
            .position = glm::make_vec4(_camera.position()),
            .frustum = last_frustum
        };

        camera_buffer().insert(camera_data);

        auto main_set = ir::descriptor_set_builder_t(*_main_pass.pipeline, 0)
            .bind_uniform_buffer(0, camera_buffer().slice())
            .bind_storage_image(1, _main_pass.visbuffer.as_const_ref().view())
            .build();

        const auto meshlet_count = _main_pass.meshlets.as_const_ref().size();

        command_buffer().begin();
        command_buffer().image_barrier({
            .image = std::cref(*_main_pass.visbuffer),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
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
            .dest_stage = ir::pipeline_stage_t::e_fragment_shader,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_shader_storage_write,
            .old_layout = ir::image_layout_t::e_transfer_dst_optimal,
            .new_layout = ir::image_layout_t::e_general,
        });
        command_buffer().begin_render_pass(*_main_pass.framebuffer, {});
        command_buffer().bind_pipeline(*_main_pass.pipeline);
        command_buffer().set_viewport({
            .width = static_cast<float32>(swapchain().width()),
            .height = static_cast<float32>(swapchain().height()),
        });
        command_buffer().set_scissor({
            .width = swapchain().width(),
            .height = swapchain().height(),
        });
        command_buffer().bind_descriptor_set(*main_set);
        {
#pragma pack(push, 1)
            struct x {
                uint64 meshlet_address;
                uint64 vertex_address;
                uint64 index_address;
                uint64 primitive_address;
                uint64 transform_address;
                uint32 meshlet_count;
            } constants = {
                _main_pass.meshlets.as_const_ref().address(),
                _main_pass.vertices.as_const_ref().address(),
                _main_pass.indices.as_const_ref().address(),
                _main_pass.primitives.as_const_ref().address(),
                _main_pass.transforms.as_const_ref().address(),
                static_cast<uint32>(meshlet_count),
            };
#pragma pack(pop)
            command_buffer().push_constants(ir::shader_stage_t::e_task | ir::shader_stage_t::e_mesh, 0, ir::size_bytes(constants), &constants);
        }
        command_buffer().draw_mesh_tasks((meshlet_count + task_workgroup_size - 1) / task_workgroup_size);
        command_buffer().end_render_pass();

        command_buffer().image_barrier({
            .image = std::cref(*_main_pass.visbuffer),
            .source_stage = ir::pipeline_stage_t::e_fragment_shader,
            .dest_stage = ir::pipeline_stage_t::e_fragment_shader,
            .source_access = ir::resource_access_t::e_shader_storage_write,
            .dest_access = ir::resource_access_t::e_shader_storage_read,
            .old_layout = ir::image_layout_t::e_general,
            .new_layout = ir::image_layout_t::e_general,
        });

        auto final_set = ir::descriptor_set_builder_t(*_final_pass.pipeline, 0)
            .bind_uniform_buffer(0, camera_buffer().slice())
            .bind_storage_image(1, _main_pass.visbuffer.as_const_ref().view())
            .build();

        command_buffer().begin_render_pass(*_final_pass.framebuffer, _final_pass.clear_values);
        command_buffer().bind_pipeline(*_final_pass.pipeline);
        command_buffer().set_viewport({
            .width = static_cast<float32>(swapchain().width()),
            .height = static_cast<float32>(swapchain().height()),
        });
        command_buffer().set_scissor({
            .width = swapchain().width(),
            .height = swapchain().height(),
        });
        command_buffer().bind_descriptor_set(*final_set);
        {
            struct {
                uint64 meshlet_address;
                uint64 vertex_address;
                uint64 index_address;
                uint64 primitive_address;
                uint64 transform_address;
            } constants = {
                _main_pass.meshlets.as_const_ref().address(),
                _main_pass.vertices.as_const_ref().address(),
                _main_pass.indices.as_const_ref().address(),
                _main_pass.primitives.as_const_ref().address(),
                _main_pass.transforms.as_const_ref().address(),
            };
            command_buffer().push_constants(ir::shader_stage_t::e_fragment, 0, ir::size_bytes(constants), &constants);
        }
        command_buffer().draw(3, 1, 0, 0);
        command_buffer().end_render_pass();
        command_buffer().image_barrier({
            .image = std::cref(swapchain().image(image_index)),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
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
            .dest_stage = ir::pipeline_stage_t::e_bottom_of_pipe,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_none,
            .old_layout = ir::image_layout_t::e_transfer_dst_optimal,
            .new_layout = ir::image_layout_t::e_present_src,
        });
        command_buffer().end();

        device().graphics_queue().submit({
            .command_buffers = { std::cref(command_buffer()) },
            .wait_semaphores = {
                { std::cref(image_available()), ir::pipeline_stage_t::e_transfer }
            },
            .signal_semaphores = {
                { std::cref(render_done()), ir::pipeline_stage_t::e_bottom_of_pipe }
            },
        }, &frame_fence());

        device().graphics_queue().present({
            .swapchain = std::cref(swapchain()),
            .wait_semaphores = {
                std::cref(render_done())
            },
            .image = image_index
        });
    }
}
