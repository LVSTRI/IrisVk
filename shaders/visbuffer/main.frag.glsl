#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "visbuffer/common.glsl"

layout (early_fragment_tests) in;

layout (location = 0) in i_vertex_data_block {
    flat uint i_meshlet_id;
};

layout (location = 0) out uint o_pixel;

void main() {
    o_pixel = (i_meshlet_id << CLUSTER_PRIMITIVE_ID_BITS) | gl_PrimitiveID;
}
