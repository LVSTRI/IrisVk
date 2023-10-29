#pragma once

#include <utilities.hpp>
#include <camera.hpp>
#include <model.hpp>

#include <iris/gfx/instance.hpp>
#include <iris/gfx/device.hpp>
#include <iris/gfx/swapchain.hpp>
#include <iris/gfx/command_pool.hpp>
#include <iris/gfx/command_buffer.hpp>
#include <iris/gfx/render_pass.hpp>
#include <iris/gfx/pipeline.hpp>
#include <iris/gfx/framebuffer.hpp>
#include <iris/gfx/queue.hpp>
#include <iris/gfx/image.hpp>
#include <iris/gfx/buffer.hpp>
#include <iris/gfx/semaphore.hpp>
#include <iris/gfx/fence.hpp>
#include <iris/gfx/texture.hpp>

#include <iris/wsi/wsi_platform.hpp>

#include <vector>
#include <chrono>
#include <queue>

#define IRIS_MAIN_VIEW_INDEX 0
#define IRIS_SHADOW_VIEW_START 1

#define IRIS_MAX_DIRECTIONAL_LIGHTS 4
#define IRIS_MAX_SPARSE_BINDING_UPDATES 16384
#define IRIS_VSM_VIRTUAL_BASE_SIZE 16384
#define IRIS_VSM_VIRTUAL_BASE_RESOLUTION (IRIS_VSM_VIRTUAL_BASE_SIZE * IRIS_VSM_VIRTUAL_BASE_SIZE)
#define IRIS_VSM_VIRTUAL_PAGE_SIZE 128
#define IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE (IRIS_VSM_VIRTUAL_BASE_SIZE / IRIS_VSM_VIRTUAL_PAGE_SIZE)
#define IRIS_VSM_VIRTUAL_PAGE_RESOLUTION (IRIS_VSM_VIRTUAL_PAGE_SIZE * IRIS_VSM_VIRTUAL_PAGE_SIZE)
#define IRIS_VSM_VIRTUAL_PAGE_COUNT (IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE * IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE)
#define IRIS_VSM_PHYSICAL_BASE_SIZE 12288
#define IRIS_VSM_PHYSICAL_BASE_RESOLUTION (IRIS_VSM_PHYSICAL_BASE_SIZE * IRIS_VSM_PHYSICAL_BASE_SIZE)
#define IRIS_VSM_PHYSICAL_PAGE_SIZE 128
#define IRIS_VSM_PHYSICAL_PAGE_ROW_SIZE (IRIS_VSM_PHYSICAL_BASE_SIZE / IRIS_VSM_PHYSICAL_PAGE_SIZE)
#define IRIS_VSM_PHYSICAL_PAGE_RESOLUTION (IRIS_VSM_PHYSICAL_PAGE_SIZE * IRIS_VSM_PHYSICAL_PAGE_SIZE)
#define IRIS_VSM_PHYSICAL_PAGE_COUNT (IRIS_VSM_PHYSICAL_PAGE_ROW_SIZE * IRIS_VSM_PHYSICAL_PAGE_ROW_SIZE)
#define IRIS_VSM_MAX_CLIPMAPS 32
#define IRIS_VSM_CLIPMAP_COUNT 16

namespace test {
    struct view_t {
        glm::mat4 projection = {};
        glm::mat4 inv_projection = {};
        glm::mat4 view = {};
        glm::mat4 inv_view = {};
        glm::mat4 stable_view = {};
        glm::mat4 inv_stable_view = {};
        glm::mat4 proj_view = {};
        glm::mat4 inv_proj_view = {};
        glm::mat4 stable_proj_view = {};
        glm::mat4 inv_stable_proj_view = {};
        glm::vec4 eye_position = {};
        glm::vec4 frustum[6] = {};
        glm::vec2 resolution = {};
    };

    struct transform_t {
        glm::mat4 model = {};
        glm::mat4 prev_model = {};
    };

    struct directional_light_t {
        glm::vec3 direction = {};
        float32 intensity = 0.0f;
    };

    struct vsm_global_data_t {
        float32 first_width = 4.0f;
        float32 lod_bias = -2.0f;
        uint32 clipmap_count = IRIS_VSM_CLIPMAP_COUNT;
    };

    template <typename T>
    struct readback_t {
        ir::arc_ptr<ir::buffer_t<T>> device;
        std::vector<ir::arc_ptr<ir::buffer_t<T>>> host;
    };

    struct hzb_t {
        ir::arc_ptr<ir::image_t> image;
        std::vector<ir::arc_ptr<ir::image_view_t>> views;
    };

    inline auto encode_page_entry(uint32 block, uint32 page) noexcept -> uint32 {
        return (block << 6) | (page & 0x3f);
    }

    inline auto decode_page_entry(uint32 entry) noexcept -> std::pair<uint32, uint32> {
        return std::make_pair(entry >> 6, entry & 0x3f);
    }

    inline auto page_entry_memory_offset(uint32 entry) noexcept -> uint64 {
        const auto [block, page] = decode_page_entry(entry);
        return (page + block * 64) * IRIS_VSM_PHYSICAL_PAGE_RESOLUTION;
    }

    template <uint64 P = IRIS_VSM_PHYSICAL_PAGE_COUNT>
    class sparse_virtual_allocator_t {
    public:
        sparse_virtual_allocator_t() noexcept = default;
        ~sparse_virtual_allocator_t() noexcept = default;

        auto allocate() noexcept -> uint32 {
            IR_PROFILE_SCOPED();
            if (is_full()) {
                return -1;
            }
            const auto [page, bit] = _search_free_page();
            _pages[page] |= 1_u64 << (63 - bit);
            _allocated += 1;
            return encode_page_entry(page, bit);
        }

        auto deallocate(const uint32& alloc) noexcept -> void {
            IR_PROFILE_SCOPED();
            if (_allocated == 0) {
                return;
            }
            const auto [block, page] = decode_page_entry(alloc);
            _pages[block] &= ~(1_u64 << (63 - page));
            _allocated -= 1;
        }

        auto is_full() noexcept -> bool {
            IR_PROFILE_SCOPED();
            return _allocated == P;
        }

        auto is_allocated(uint32 index) noexcept -> bool {
            IR_PROFILE_SCOPED();
            const auto block = index / 64;
            const auto page = index % 64;
            return (_pages[block] & (1_u64 << (63 - page))) != 0;
        }

    private:
        auto _search_free_page() noexcept -> std::pair<uint64, uint64> {
            IR_PROFILE_SCOPED();
            for (auto i = 0_u64; i < _pages.size(); ++i) {
                const auto page = _pages[i];
                if (page == ~0_u64) {
                    continue;
                }
                const auto index = std::countl_zero(~page);
                return std::make_pair(i, index);
            }
            IR_UNREACHABLE();
        }

        std::array<uint64, P / 64> _pages = {};
        uint64 _allocated = 0;
    };

    class virtual_shadow_map_t {
    public:
        using self = virtual_shadow_map_t;

        virtual_shadow_map_t(sparse_virtual_allocator_t<>& allocator) noexcept
            : _allocator(std::ref(allocator)) {
            IR_PROFILE_SCOPED();
        }

        ~virtual_shadow_map_t() noexcept = default;

        static auto make(
            const ir::device_t& device,
            sparse_virtual_allocator_t<>& allocator,
            ir::buffer_t<float32>& buffer
        ) noexcept -> self {
            IR_PROFILE_SCOPED();
            auto vsm = self(allocator);
            vsm._image = ir::image_t::make(device, {
                .width = IRIS_VSM_VIRTUAL_BASE_SIZE,
                .height = IRIS_VSM_VIRTUAL_BASE_SIZE,
                .usage =
                    ir::image_usage_t::e_depth_stencil_attachment |
                    ir::image_usage_t::e_sampled,
                .flags =
                    ir::image_flag_t::e_sparse_binding |
                    ir::image_flag_t::e_sparse_residency,
                .format = ir::resource_format_t::e_d32_sfloat,
                .view = ir::default_image_view_info,
            });
            vsm._pages.resize(IRIS_VSM_VIRTUAL_PAGE_COUNT, -1_u32);
            vsm._buffer = buffer.as_intrusive_ptr();
            return vsm;
        }

        auto make_updated_sparse_bindings(std::span<const uint8> requests) noexcept -> std::vector<ir::sparse_image_memory_bind_t> {
            IR_PROFILE_SCOPED();
            auto bindings = std::vector<ir::sparse_image_memory_bind_t>();
            bindings.reserve(requests.size());
            for (auto page = 0_u64; page < requests.size(); ++page) {
                const auto offset = ir::offset_3d_t {
                    .x = static_cast<int32>((page % IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE) * IRIS_VSM_VIRTUAL_PAGE_SIZE),
                    .y = static_cast<int32>((page / IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE) * IRIS_VSM_VIRTUAL_PAGE_SIZE),
                };
                const auto extent = ir::extent_3d_t {
                    .width = IRIS_VSM_VIRTUAL_PAGE_SIZE,
                    .height = IRIS_VSM_VIRTUAL_PAGE_SIZE,
                    .depth = 1,
                };
                const auto is_requested = requests[page] != 0;
                const auto is_allocated = _pages[page] != -1_u32;
                if (is_requested && !is_allocated) {
                    const auto entry = _allocator.get().allocate();
                    if (entry == -1_u32) {
                        continue;
                    }
                    const auto memory_offset = page_entry_memory_offset(entry);
                    bindings.emplace_back(ir::sparse_image_memory_bind_t {
                        .offset = offset,
                        .extent = extent,
                        .buffer = _buffer->slice(memory_offset, IRIS_VSM_PHYSICAL_PAGE_RESOLUTION, false),
                    });
                    _pages[page] = entry;
                    _allocated++;
                } else if (!is_requested && is_allocated) {
                    const auto entry = _pages[page];
                    const auto memory_offset = page_entry_memory_offset(entry);
                    bindings.emplace_back(ir::sparse_image_memory_bind_t {
                        .offset = offset,
                        .extent = extent,
                        .buffer = _buffer->slice(memory_offset, IRIS_VSM_PHYSICAL_PAGE_RESOLUTION, true),
                    });
                    _allocator.get().deallocate(entry);
                    _pages[page] = -1_u32;
                    _allocated--;
                }
            }
            return bindings;
        }

        auto image() noexcept -> ir::image_t& {
            IR_PROFILE_SCOPED();
            return *_image;
        }

        auto image() const noexcept -> const ir::image_t& {
            IR_PROFILE_SCOPED();
            return *_image;
        }

        auto is_empty() const noexcept -> bool {
            IR_PROFILE_SCOPED();
            return _allocated == 0;
        }

    private:
        ir::arc_ptr<ir::image_t> _image;
        ir::arc_ptr<ir::buffer_t<float32>> _buffer;
        ir::arc_ptr<const ir::device_t> _device;
        std::reference_wrapper<sparse_virtual_allocator_t<>> _allocator;

        std::vector<uint32> _pages;
        uint32 _allocated = 0;
    };

    class application_t {
    public:
        application_t() noexcept;
        ~application_t() noexcept;

        auto run() noexcept -> void;

    private:
        auto _load_models() noexcept -> void;

        auto _current_frame_data() noexcept;
        auto _current_frame_buffers() noexcept;

        auto _render() noexcept -> void;
        auto _update() noexcept -> void;
        auto _update_frame_data() noexcept -> void;
        auto _update_sparse_bindings() noexcept -> void;
        auto _resize() noexcept -> void;

        auto _initialize() noexcept -> void;
        auto _initialize_sync() noexcept -> void;
        auto _initialize_visbuffer_pass() noexcept -> void;
        auto _initialize_vsm_pass() noexcept -> void;

        auto _clear_buffer_pass() noexcept -> void;
        auto _visbuffer_pass() noexcept -> void;
        auto _visbuffer_resolve_pass() noexcept -> void;
        auto _visbuffer_tonemap_pass() noexcept -> void;
        auto _vsm_mark_visible_pages_pass() noexcept -> void;
        auto _vsm_hardware_rasterize_pass() noexcept -> void;
        auto _swapchain_copy_pass(uint32 image_index) noexcept -> void;

        uint64 _frame_index = 0;
        ir::wsi_platform_t _wsi;
        ir::arc_ptr<ir::instance_t> _instance;
        ir::arc_ptr<ir::device_t> _device;
        ir::arc_ptr<ir::swapchain_t> _swapchain;
        std::vector<ir::arc_ptr<ir::command_pool_t>> _command_pools;
        std::vector<ir::arc_ptr<ir::command_buffer_t>> _command_buffers;
        std::vector<ir::arc_ptr<ir::semaphore_t>> _image_available;
        std::vector<ir::arc_ptr<ir::semaphore_t>> _render_done;
        std::vector<ir::arc_ptr<ir::fence_t>> _frame_fence;
        std::vector<ir::arc_ptr<ir::fence_t>> _sparse_fence;
        ir::arc_ptr<ir::semaphore_t> _sparse_bind_semaphore;

        struct {
            bool is_initialized = false;
            ir::arc_ptr<ir::image_t> ids;
            ir::arc_ptr<ir::image_t> depth;
            ir::arc_ptr<ir::image_t> color;
            ir::arc_ptr<ir::image_t> final;
            ir::arc_ptr<ir::render_pass_t> pass;
            ir::arc_ptr<ir::framebuffer_t> framebuffer;
            ir::arc_ptr<ir::pipeline_t> main;
            ir::arc_ptr<ir::pipeline_t> resolve;
            ir::arc_ptr<ir::pipeline_t> tonemap;
        } _visbuffer;

        struct {
            bool is_initialized = false;
            ir::arc_ptr<ir::pipeline_t> mark_visible_pages;
            ir::arc_ptr<ir::pipeline_t> rasterize_hardware;
            ir::arc_ptr<ir::render_pass_t> rasterize_pass;
            std::vector<ir::arc_ptr<ir::framebuffer_t>> rasterize_framebuffers;

            sparse_virtual_allocator_t<> allocator;
            ir::arc_ptr<ir::buffer_t<float32>> memory;

            hzb_t hzb;
            std::vector<virtual_shadow_map_t> images;
            std::vector<std::reference_wrapper<const ir::image_view_t>> image_views;
            ir::arc_ptr<ir::sampler_t> sampler;

            std::queue<ir::queue_bind_sparse_info_t> sparse_binding_deferred_queue;

            readback_t<uint8> visible_pages_buffer;

            std::vector<ir::arc_ptr<ir::buffer_t<vsm_global_data_t>>> globals_buffer;
        } _vsm;

        struct {
            std::vector<ir::arc_ptr<ir::buffer_t<view_t>>> views;
            std::vector<ir::arc_ptr<ir::buffer_t<transform_t>>> transforms;
            std::vector<ir::arc_ptr<ir::buffer_t<material_t>>> materials;
            std::vector<ir::arc_ptr<ir::buffer_t<directional_light_t>>> directional_lights;
            ir::arc_ptr<ir::buffer_t<base_meshlet_t>> meshlets;
            ir::arc_ptr<ir::buffer_t<meshlet_instance_t>> meshlet_instances;
            ir::arc_ptr<ir::buffer_t<meshlet_vertex_format_t>> vertices;
            ir::arc_ptr<ir::buffer_t<uint32>> indices;
            ir::arc_ptr<ir::buffer_t<uint8>> primitives;
            ir::arc_ptr<ir::buffer_t<uint64>> atomics;
        } _buffer;

        struct scene_t {
            std::vector<ir::arc_ptr<ir::texture_t>> textures;
            std::vector<material_t> materials;
        } _scene;

        struct state_t {
            vsm_global_data_t vsm_global_data = {};
            std::vector<directional_light_t> directional_lights = {};
        } _state;

        camera_t _camera;

        ch::steady_clock::time_point _last_frame = {};
        float32 _delta_time = 0.0f;
    };
}