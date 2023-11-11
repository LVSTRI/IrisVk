#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "utilities.glsl"
#include "buffer.glsl"

#define INVOCATION_SIZE 16
#define INVOCATION_THREADS (INVOCATION_SIZE * INVOCATION_SIZE)

layout (local_size_x = INVOCATION_SIZE, local_size_y = INVOCATION_SIZE) in;

layout (rg32f, set = 0, binding = 0) restrict readonly uniform image2D u_depth_in;
layout (rg32f, set = 0, binding = 1) restrict writeonly uniform image2D u_depth_out;

shared vec2 s_depth[INVOCATION_THREADS];

void main() {
    const ivec2 position = min(ivec2(gl_GlobalInvocationID.xy), imageSize(u_depth_in) - 1);
    const vec2 d_sample = imageLoad(u_depth_in, position).xy;
    s_depth[gl_LocalInvocationIndex] = vec2(d_sample.x, d_sample.y);
    barrier();

    for (uint i = INVOCATION_THREADS >> 1; i > 0; i >>= 1) {
        if (gl_LocalInvocationIndex < i) {
            const vec2 d0 = s_depth[gl_LocalInvocationIndex];
            const vec2 d1 = s_depth[gl_LocalInvocationIndex + i];
            s_depth[gl_LocalInvocationIndex] = vec2(min(d0.x, d1.x), max(d0.y, d1.y));
        }
        barrier();
    }

    if (gl_LocalInvocationIndex == 0) {
        imageStore(u_depth_out, ivec2(gl_WorkGroupID.xy), vec4(s_depth[0], 0, 0));
    }
}
