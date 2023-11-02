#version 460
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#include "vsm/common.glsl"

layout (local_size_x = 16, local_size_y = 16) in;

layout (set = 0, binding = 0) uniform texture2D u_depth;

layout (scalar, push_constant) restrict uniform u_push_constant_block {
    restrict b_view_block u_view_ptr;
    restrict b_vsm_globals_block u_vsm_globals_ptr;
    restrict writeonly b_vsm_page_request_block u_page_request_ptr;
};

void main() {
    const uvec2 resolution = textureSize(u_depth, 0);
    const uvec2 position = gl_GlobalInvocationID.xy;
    if (any(greaterThanEqual(position, resolution))) {
        return;
    }

    const float depth = texelFetch(u_depth, ivec2(position), 0).r;
    if (depth == 0.0) {
        return;
    }

    const mat4 inv_proj_view = u_view_ptr.data[IRIS_MAIN_VIEW_INDEX].inv_proj_view;
    const virtual_page_info_t virtual_page = virtual_page_info_from_depth(
        u_view_ptr,
        u_vsm_globals_ptr,
        vec2(resolution),
        inv_proj_view,
        position,
        depth
    );
    const uvec2 virtual_page_position = clamp(virtual_page.position.xy, uvec2(0), uvec2(IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE - 1));
    const uint virtual_page_index = virtual_page_position.x + virtual_page_position.y * IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE;
    u_page_request_ptr.data[virtual_page_index + virtual_page.clipmap_level * IRIS_VSM_VIRTUAL_PAGE_COUNT] = uint8_t(1);
}
