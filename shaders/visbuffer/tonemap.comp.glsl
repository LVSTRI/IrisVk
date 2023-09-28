#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_shader_subgroup : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "utilities.glsl"

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout (rgba32f, set = 0, binding = 0) uniform restrict readonly image2D u_input;
layout (rgba8, set = 0, binding = 1) uniform restrict writeonly image2D u_output;

vec3 tonemap(in vec3 color) {
    const float l = luminance(color);
    const vec3 reinhard = color / (1.0 + color);
    return mix(color / (1.0 + l), reinhard, reinhard);
}

void main() {
    const ivec2 size = imageSize(u_input);
    if (gl_GlobalInvocationID.x >= size.x || gl_GlobalInvocationID.y >= size.y) {
        return;
    }
    const vec4 payload = imageLoad(u_input, ivec2(gl_GlobalInvocationID.xy));
    imageStore(u_output, ivec2(gl_GlobalInvocationID.xy), vec4(linear_as_srgb(tonemap(payload.xyz)), 1.0));
}