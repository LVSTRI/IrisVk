#ifndef IRIS_VSM_COMMON_HEADER
#define IRIS_VSM_COMMON_HEADER

#define IRIS_VSM_VIRTUAL_BASE_SIZE 16384
#define IRIS_VSM_VIRTUAL_PAGE_SIZE 128
#define IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE (IRIS_VSM_VIRTUAL_BASE_SIZE / IRIS_VSM_VIRTUAL_PAGE_SIZE)
#define IRIS_VSM_VIRTUAL_PAGE_COUNT (IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE * IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE)
#define IRIS_VSM_PHYSICAL_BASE_SIZE 8192
#define IRIS_VSM_PHYSICAL_PAGE_SIZE 128
#define IRIS_VSM_PHYSICAL_PAGE_ROW_SIZE (IRIS_VSM_PHYSICAL_BASE_SIZE / IRIS_VSM_PHYSICAL_PAGE_SIZE)
#define IRIS_VSM_PHYSICAL_PAGE_COUNT (IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE * IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE)
#define IRIS_VSM_MAX_CLIPMAPS 32

#include "buffer.glsl"
#include "utilities.glsl"

struct virtual_page_info_t {
    uvec3 position;
    vec3 uv;
};

uint encode_physical_page_entry(in uvec2 page) {
    return page.x << 5u | page.y;
}

uvec2 decode_physical_page_entry(in uint page) {
    return uvec2(
        page >> 5u,
        page & 0x1fu
    );
}

// [physical_page]: in range [0; VSM_PHYSICAL_PAGE_COUNT)
uvec2 calculate_physical_page_texel_corner(in uint physical_page) {
    const uvec2 entry = decode_physical_page_entry(physical_page);
    const uint index = entry.x * 32 + entry.y;
    const uint x = index % IRIS_VSM_PHYSICAL_PAGE_ROW_SIZE;
    const uint y = index / IRIS_VSM_PHYSICAL_PAGE_ROW_SIZE;
    return uvec2(
        x * IRIS_VSM_PHYSICAL_PAGE_SIZE,
        y * IRIS_VSM_PHYSICAL_PAGE_SIZE
    );
}

// [virtual_texel]: in range [0; VSM_VIRTUAL_BASE_SIZE)
uvec2 virtual_to_physical_texel(in uvec2 virtual_texel) {
    return virtual_texel % IRIS_VSM_PHYSICAL_PAGE_SIZE;
}

// From: https://github.com/JuanDiegoMontoya/Frogfood/blob/main/data/shaders/shadows/vsm/VsmCommon.h.glsl#L174C70-L174C73
virtual_page_info_t virtual_page_info_from_depth(
    in restrict b_view_block view_ptr,
    in restrict b_vsm_globals_block vsm_globals_ptr,
    in vec2 resolution,
    in mat4 inv_proj_view,
    in uvec2 texel,
    in float depth
) {
    const vsm_global_data_t vsm_data = vsm_globals_ptr.data;
    const float first_clipmap_texel_width = vsm_data.first_width / IRIS_VSM_VIRTUAL_BASE_SIZE;
    const vec2 texel_size = 1.0 / resolution;
    const vec2 uv_center = (vec2(texel) + 0.5) * texel_size;
    const vec2 uv_left = uv_center + vec2(-texel_size.x, 0.0) * 0.5;
    const vec2 uv_right = uv_center + vec2(texel_size.x, 0.0) * 0.5;
    const vec3 world_position = uv_to_world(inv_proj_view, uv_center, depth);
    const vec3 world_left = uv_to_world(inv_proj_view, uv_left, depth);
    const vec3 world_right = uv_to_world(inv_proj_view, uv_right, depth);
    const float projection_length = distance(world_left, world_right);
    const uint unclamped_clipmap_level = uint(max(ceil(log2(projection_length / first_clipmap_texel_width) + vsm_data.lod_bias), 0.0));
    const uint clipmap_level = clamp(unclamped_clipmap_level, 0, vsm_data.clipmap_count - 1);
    const mat4 clipmap_proj_view = view_ptr.data[IRIS_SHADOW_VIEW_START_INDEX + clipmap_level].proj_view;
    const vec4 shadow_position = clipmap_proj_view * vec4(world_position, 1.0);
    const vec2 virtual_shadow_uv = fract(shadow_position.xy * 0.5 + 0.5);
    const uvec2 virtual_shadow_page = uvec2(virtual_shadow_uv * IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE);

    return virtual_page_info_t(
        uvec3(virtual_shadow_page, clipmap_level),
        vec3(virtual_shadow_uv, shadow_position.z)
    );
}

bool is_virtual_page_backed(in uint virtual_page) {
    return virtual_page != uint(-1);
}

#endif