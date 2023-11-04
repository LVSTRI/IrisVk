#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable

#include "vsm/common.glsl"
#include "buffer.glsl"
#include "utilities.glsl"

layout (r32ui, set = 0, binding = 0) restrict uniform uimage2D u_vsm;

layout (scalar, push_constant) restrict uniform u_push_constants_block {
    restrict readonly b_view_block u_view_ptr;
    restrict readonly b_meshlet_instance_block u_meshlet_instance_ptr;
    restrict readonly b_meshlet_block u_meshlet_ptr;
    restrict readonly b_transform_block u_transform_ptr;
    restrict readonly b_vertex_block u_vertex_ptr;
    restrict readonly b_index_block u_index_ptr;
    restrict readonly b_primitive_block u_primitive_ptr;
    restrict readonly b_vsm_virtual_page_table_block u_virt_page_table_ptr;

    uint u_view_index;
};

void main() {
    const uvec2 position = uvec2(gl_FragCoord.xy);
    const uint depth = floatBitsToUint(gl_FragCoord.z);
    const uvec2 virtual_page = position / IRIS_VSM_VIRTUAL_PAGE_SIZE;
    const uint virtual_page_index = virtual_page.x + virtual_page.y * IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE;
    const uint virtual_page_entry = u_virt_page_table_ptr.data[virtual_page_index + u_view_index * IRIS_VSM_VIRTUAL_PAGE_COUNT];
    if (is_virtual_page_backed(virtual_page_entry)) {
        const uvec2 physical_page_corner = calculate_physical_page_texel_corner(virtual_page_entry);
        const uvec2 physical_texel = physical_page_corner + (position % IRIS_VSM_PHYSICAL_PAGE_SIZE);
        imageAtomicMin(u_vsm, ivec2(physical_texel), depth);
    }
}
