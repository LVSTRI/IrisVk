#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_shader_subgroup : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#define IRIS_TONEMAP_TYPE_REINHARD_LUMINANCE 0
#define IRIS_TONEMAP_TYPE_PASSTHROUGH 1
#define IRIS_TONEMAP_TYPE IRIS_TONEMAP_TYPE_PASSTHROUGH

#include "utilities.glsl"

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout (rgba32f, set = 0, binding = 0) uniform restrict readonly image2D u_input;
layout (rgba8, set = 0, binding = 1) uniform restrict writeonly image2D u_output;


vec3 tonemap(in vec3 color) {
#if IRIS_TONEMAP_TYPE == IRIS_TONEMAP_TYPE_REINHARD_LUMINANCE
    const float lum = luminance(color);
    const vec3 reinhard = color / (color + 1.0);
    return mix(color / (lum + 1.0), reinhard, reinhard);
#elif IRIS_TONEMAP_TYPE == IRIS_TONEMAP_TYPE_PASSTHROUGH
    return clamp(color, 0.0, 1.0);
#endif
}

void main() {
    const ivec2 size = imageSize(u_input);
    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, size))) {
        return;
    }
    const ivec2 position = ivec2(gl_GlobalInvocationID.xy);
    const vec4 payload = imageLoad(u_input, position);
    imageStore(u_output, position, vec4(linear_as_srgb(tonemap(payload.xyz)), 1.0));
}