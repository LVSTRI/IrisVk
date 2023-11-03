#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "vsm/common.glsl"
#include "buffer.glsl"
#include "utilities.glsl"

layout (local_size_x = 16, local_size_y = 16) in;

layout (scalar, push_constant) restrict uniform u_push_constant_block {
    restrict readonly b_vsm_page_request_block u_page_request_ptr;
    restrict b_vsm_physical_page_table_block u_phys_page_table_ptr;
    restrict b_vsm_virtual_page_table_block u_virt_page_table_ptr;
};

void main() {
    const uvec2 position = uvec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(position, uvec2(IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE)))) {
        return;
    }
    const uint clipmap = gl_WorkGroupID.z;
    const uint virtual_index = position.x + position.y * IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE;
    const uint virtual_clipmap_index = virtual_index + clipmap * IRIS_VSM_VIRTUAL_PAGE_COUNT;
    const uint virtual_page_entry = u_virt_page_table_ptr.data[virtual_clipmap_index];
    const bool is_page_allocated = is_virtual_page_backed(virtual_page_entry);
    const bool is_page_requested = u_page_request_ptr.data[virtual_clipmap_index] == 1;

    if (is_page_allocated && !is_page_requested) {
        const uvec2 physical_page_entry = decode_physical_page_entry(virtual_page_entry);
        atomicAnd(u_phys_page_table_ptr.data[physical_page_entry.x], ~(1u << physical_page_entry.y));
        u_virt_page_table_ptr.data[virtual_clipmap_index] = uint(-1);
    }
}
