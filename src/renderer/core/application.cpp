#include <iris/gfx/device.hpp>
#include <iris/gfx/instance.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/command_pool.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/framebuffer.hpp>
#include <iris/gfx/pipeline.hpp>
#include <iris/gfx/fence.hpp>
#include <iris/gfx/clear_value.hpp>
#include <iris/gfx/semaphore.hpp>
#include <iris/gfx/queue.hpp>
#include <iris/gfx/swapchain.hpp>
#include <iris/gfx/render_pass.hpp>

#include <renderer/core/application.hpp>

namespace app {
    application_t::application_t() noexcept {
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
        _swapchain = ir::swapchain_t::make(*_device, _platform, {});
        _main_pass.description = ir::render_pass_t::make(*_device, {
            .attachments = {
                {
                    .layout = {
                        .final = ir::image_layout_t::e_transfer_src_optimal
                    },
                    .format = _swapchain.as_const_ref().format(),
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
            .width = _swapchain.as_const_ref().width(),
            .height = _swapchain.as_const_ref().height(),
            .usage = ir::image_usage_t::e_color_attachment | ir::image_usage_t::e_transfer_src,
            .view = ir::default_image_view_info,
        });
        _main_pass.depth = ir::image_t::make_from_attachment(*_device, _main_pass.description.as_const_ref().attachment(1), {
            .width = _swapchain.as_const_ref().width(),
            .height = _swapchain.as_const_ref().height(),
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

        _main_pass.main_pipeline = ir::pipeline_t::make(*_main_pass.framebuffer, {
            .vertex = "../shaders/0.1/main.vert",
            .fragment = "../shaders/0.1/main.frag",
            .blend = {
                ir::attachment_blend_t::e_disabled
            },
            .dynamic_states = {},
            .depth_flags =
                ir::depth_state_flag_t::e_enable_test |
                ir::depth_state_flag_t::e_enable_write,
            .depth_compare_op = ir::compare_op_t::e_greater,
            .cull_mode = ir::cull_mode_t::e_none,
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
        _device.as_const_ref().wait_idle();
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

    auto application_t::_update() noexcept -> void {
        IR_PROFILE_SCOPED();
        ir::wsi_platform_t::poll_events();

        const auto current_time = ch::steady_clock::now();
        _delta_time = ch::duration<float32>(current_time - _last_time).count();
        _last_time = current_time;
        _frame_count++;
    }

    auto application_t::_render() noexcept -> void {
        IR_PROFILE_SCOPED();
        const auto& fence = *_frame_fence[_frame_index];
        const auto& image_semaphore = *_image_available[_frame_index];
        const auto& render_semaphore = *_render_done[_frame_index];
        auto& command_buffer = *_command_buffers[_frame_index];

        fence.wait();
        fence.reset();
        command_buffer.pool().reset();
        const auto image_index = _swapchain.as_const_ref().acquire_next_image(image_semaphore);

        command_buffer.begin();
        command_buffer.begin_render_pass(*_main_pass.framebuffer, _main_pass.clear_values);
        command_buffer.bind_pipeline(*_main_pass.main_pipeline);
        command_buffer.draw(3, 1, 0, 0);
        command_buffer.end_render_pass();
        command_buffer.image_barrier({
            .image = std::cref(_swapchain.as_const_ref().image(image_index)),
            .source_stage = ir::pipeline_stage_t::e_top_of_pipe,
            .dest_stage = ir::pipeline_stage_t::e_transfer,
            .source_access = ir::resource_access_t::e_none,
            .dest_access = ir::resource_access_t::e_transfer_write,
            .old_layout = ir::image_layout_t::e_undefined,
            .new_layout = ir::image_layout_t::e_transfer_dst_optimal,
        });
        command_buffer.copy_image({
            .source = std::cref(_main_pass.color.as_const_ref()),
            .dest = std::cref(_swapchain.as_const_ref().image(image_index)),
        });
        command_buffer.image_barrier({
            .image = std::cref(_swapchain.as_const_ref().image(image_index)),
            .source_stage = ir::pipeline_stage_t::e_transfer,
            .dest_stage = ir::pipeline_stage_t::e_bottom_of_pipe,
            .source_access = ir::resource_access_t::e_transfer_write,
            .dest_access = ir::resource_access_t::e_none,
            .old_layout = ir::image_layout_t::e_transfer_dst_optimal,
            .new_layout = ir::image_layout_t::e_present_src,
        });
        command_buffer.end();

        _device.as_const_ref().graphics_queue().submit({
            .command_buffers = { std::cref(command_buffer) },
            .wait_semaphores = {
                { std::cref(image_semaphore), ir::pipeline_stage_t::e_transfer }
            },
            .signal_semaphores = {
                { std::cref(render_semaphore), ir::pipeline_stage_t::e_transfer }
            },
        }, &fence);

        _device.as_const_ref().graphics_queue().present({
            .swapchain = std::cref(_swapchain.as_const_ref()),
            .wait_semaphores = {
                std::cref(render_semaphore)
            },
            .image = image_index
        });
    }
}
