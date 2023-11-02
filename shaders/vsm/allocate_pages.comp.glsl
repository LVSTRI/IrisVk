#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_KHR_memory_scope_semantics : enable
#pragma use_vulkan_memory_model

#include "vsm/common.glsl"
#include "buffer.glsl"
#include "utilities.glsl"

layout (local_size_x = 1) in;

layout (scalar, push_constant) restrict uniform u_push_constant_block {
    restrict queuefamilycoherent b_vsm_physical_page_table_block u_phys_page_table_ptr;
    restrict queuefamilycoherent b_vsm_virtual_page_table_block u_virt_page_table_ptr;
    restrict readonly b_vsm_allocate_request_block u_allocation_request_ptr;
};

void main() {
    for (uint i = 0; i < u_allocation_request_ptr.count; i++) {
        const uint page_request = u_allocation_request_ptr.data[i];
        const uint virtual_clipmap_index = page_request >> 1;
        const bool is_allocation = (page_request & 0x1u) == 1;
        if (is_allocation) {
            // find first free bit in physical page table
            // FIXME [WARNING]: suppose allocation never fails
            const uint mask_count = IRIS_VSM_PHYSICAL_PAGE_COUNT / 32;
            for (uint j = 0; j < mask_count; ++j) {
                const uint mask = u_phys_page_table_ptr.data[j];
                if (mask == ~0u) {
                    continue;
                }
                const uint bit = findLSB(~mask);
                u_phys_page_table_ptr.data[j] |= 1u << bit;
                u_virt_page_table_ptr.data[virtual_clipmap_index] = encode_physical_page_entry(uvec2(j, bit));
                break;
            }
        } else {
            const uvec2 physical_page_entry = decode_physical_page_entry(u_virt_page_table_ptr.data[virtual_clipmap_index]);
            u_phys_page_table_ptr.data[physical_page_entry.x] &= ~(1u << physical_page_entry.y);
            u_virt_page_table_ptr.data[virtual_clipmap_index] = uint(-1);
        }
    }
}
