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
    application_t::application_t() noexcept
        : _camera(_platform) {
        IR_PROFILE_SCOPED();
        _platform = ir::wsi_platform_t::make(1280, 720, "Iris");
        _instance = ir::instance_t::make({
            .features = {
                .debug_markers = true
            },
            .wsi_extensions = ir::wsi_platform_t::context_extensions()
        });
        _device = ir::device_t::make(*_instance, {
            .features = {
                .swapchain = true
            }
        });
        _swapchain = ir::swapchain_t::make(*_device, _platform, {
            .vsync = false
        });
        _main_pass.description = ir::render_pass_t::make(*_device, {
            .attachments = {
                {
                    .layout = {
                        .final = ir::image_layout_t::e_transfer_src_optimal
                    },
                    .format = swapchain().format(),
                    .load_op = ir::attachment_load_op_t::e_clear,
                    .store_op = ir::attachment_store_op_t::e_store,
                }, {
                    .layout = {
                        .final = ir::image_layout_t::e_depth_stencil_attachment_optimal
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
                        ir::pipeline_stage_t::e_early_fragment_tests,
                    .dest_stage =
                        ir::pipeline_stage_t::e_color_attachment_output |
                        ir::pipeline_stage_t::e_early_fragment_tests,
                    .source_access = ir::resource_access_t::e_none,
                    .dest_access =
                        ir::resource_access_t::e_color_attachment_write |
                        ir::resource_access_t::e_depth_stencil_attachment_write
                }, {
                    .source = 0,
                    .dest = ir::external_subpass,
                    .source_stage =
                        ir::pipeline_stage_t::e_color_attachment_output |
                        ir::pipeline_stage_t::e_early_fragment_tests,
                    .dest_stage = ir::pipeline_stage_t::e_transfer,
                    .source_access =
                        ir::resource_access_t::e_color_attachment_write |
                        ir::resource_access_t::e_depth_stencil_attachment_write,
                    .dest_access = ir::resource_access_t::e_transfer_read
                }
            }
        });
        _main_pass.color = ir::image_t::make_from_attachment(*_device, _main_pass.description.as_const_ref().attachment(0), {
            .width = swapchain().width(),
            .height = swapchain().height(),
            .usage = ir::image_usage_t::e_color_attachment | ir::image_usage_t::e_transfer_src,
            .view = ir::default_image_view_info,
        });
        _main_pass.depth = ir::image_t::make_from_attachment(*_device, _main_pass.description.as_const_ref().attachment(1), {
            .width = swapchain().width(),
            .height = swapchain().height(),
            .usage = ir::image_usage_t::e_depth_stencil_attachment,
            .view = ir::default_image_view_info,
        });
        _main_pass.framebuffer = ir::framebuffer_t::make(*_main_pass.description, {
            .attachments = { _main_pass.color, _main_pass.depth },
        });
        _main_pass.clear_values = {
            ir::make_clear_color({ 0.597f, 0.808f, 1.0f, 1.0f }),
            ir::make_clear_depth(0.0f, 0)
        };
        _main_pass.main_pipeline = ir::pipeline_t::make(*_device, *_main_pass.framebuffer, {
            .vertex = "../shaders/0.1/main.vert",
            .fragment = "../shaders/0.1/main.frag",
            .blend = {
                ir::attachment_blend_t::e_disabled
            },
            .dynamic_states = {
                ir::dynamic_state_t::e_viewport,
                ir::dynamic_state_t::e_scissor
            },
            .depth_flags =
                ir::depth_state_flag_t::e_enable_test |
                ir::depth_state_flag_t::e_enable_write,
            .depth_compare_op = ir::compare_op_t::e_greater,
            .cull_mode = ir::cull_mode_t::e_none,
        });
        _main_pass.camera_buffer = ir::buffer_t<camera_data_t>::make(*_device, frames_in_flight, {
            .usage = ir::buffer_usage_t::e_uniform_buffer,
            .flags = ir::buffer_flag_t::e_mapped,
            .capacity = sizeof(camera_data_t),
        });

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
        return *_command_pools[frame_index == -1 ? _frame_index : frame_index];
    }

    auto application_t::command_buffer(uint32 frame_index) noexcept -> ir::command_buffer_t& {
        IR_PROFILE_SCOPED();
        return *_command_buffers[frame_index == -1 ? _frame_index : frame_index];
    }

    auto application_t::image_available(uint32 frame_index) noexcept -> ir::semaphore_t& {
        IR_PROFILE_SCOPED();
        return *_image_available[frame_index == -1 ? _frame_index : frame_index];
    }

    auto application_t::render_done(uint32 frame_index) noexcept -> ir::semaphore_t& {
        IR_PROFILE_SCOPED();
        return *_render_done[frame_index == -1 ? _frame_index : frame_index];
    }

    auto application_t::frame_fence(uint32 frame_index) noexcept -> ir::fence_t& {
        IR_PROFILE_SCOPED();
        return *_frame_fence[frame_index == -1 ? _frame_index : frame_index];
    }

    auto application_t::main_pass() noexcept -> main_pass_t& {
        IR_PROFILE_SCOPED();
        return _main_pass;
    }

    auto application_t::camera_buffer(uint32 frame_index) noexcept -> ir::buffer_t<camera_data_t>& {
        IR_PROFILE_SCOPED();
        return *_main_pass.camera_buffer[frame_index == -1 ? _frame_index : frame_index];
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

        camera_buffer().insert({
            .projection = _camera.projection(),
            .view = _camera.view(),
            .pv = _camera.projection() * _camera.view(),
            .position = glm::make_vec4(_camera.position()),
        });

        auto main_set = ir::descriptor_set_builder_t(*main_pass().main_pipeline, 0)
            .bind_uniform_buffer(0, camera_buffer().slice())
            .build();

        command_buffer().begin();
        command_buffer().begin_render_pass(*_main_pass.framebuffer, _main_pass.clear_values);
        command_buffer().bind_pipeline(*_main_pass.main_pipeline);
        command_buffer().set_viewport({
            .width = static_cast<float32>(swapchain().width()),
            .height = static_cast<float32>(swapchain().height()),
        });
        command_buffer().set_scissor({
            .width = swapchain().width(),
            .height = swapchain().height(),
        });
        command_buffer().bind_descriptor_set(*main_set);
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
        command_buffer().copy_image({
            .source = std::cref(*_main_pass.color),
            .dest = std::cref(swapchain().image(image_index)),
        });
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
