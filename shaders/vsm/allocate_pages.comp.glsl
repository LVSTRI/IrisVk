#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "vsm/common.glsl"
#include "buffer.glsl"
#include "utilities.glsl"

layout (local_size_x = 1) in;

layout (scalar, push_constant) restrict uniform u_push_constant_block {
    restrict readonly b_vsm_allocate_request_block u_allocation_request_ptr;
    restrict b_vsm_physical_page_table_block u_phys_page_table_ptr;
    restrict b_vsm_virtual_page_table_block u_virt_page_table_ptr;
};

void main() {
    for (uint i = 0; i < u_allocation_request_ptr.count; ++i) {
        const uint virtual_clipmap_index = u_allocation_request_ptr.data[i];
        // find first free bit in physical page table
        // FIXME [WARNING]: suppose allocation never fails
        for (uint j = 0; j < IRIS_VSM_PHYSICAL_PAGE_COUNT / 32; ++j) {
            const uint mask = u_phys_page_table_ptr.data[j];
            if (mask == uint(-1)) {
                continue;
            }
            const uint bit = findLSB(~mask);
            u_phys_page_table_ptr.data[j] |= 1u << bit;
            u_virt_page_table_ptr.data[virtual_clipmap_index] = encode_physical_page_entry(uvec2(j, bit));
            break;
        }
    }
}

