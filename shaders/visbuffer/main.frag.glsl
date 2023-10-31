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
    vec4 i_clip_position;
    vec4 i_prev_clip_position;
    vec2 i_resolution;
};

layout (location = 0) out uint o_pixel;
layout (location = 1) out vec2 o_velocity;

vec2 make_motion_vector() {
    const vec4 ndc_position = i_clip_position / i_clip_position.w;
    const vec4 ndc_prev_position = i_prev_clip_position / i_prev_clip_position.w;
    const vec2 uv_position = clamp(ndc_position.xy, -1.0, 1.0) * 0.5 + 0.5;
    const vec2 uv_prev_position = clamp(ndc_prev_position.xy, -1.0, 1.0) * 0.5 + 0.5;
    return (uv_position - uv_prev_position) * i_resolution;
}

void main() {
    o_pixel = (i_meshlet_id << CLUSTER_PRIMITIVE_ID_BITS) | gl_PrimitiveID;
    o_velocity = -make_motion_vector();
}
