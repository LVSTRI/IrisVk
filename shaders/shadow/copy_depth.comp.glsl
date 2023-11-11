#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "utilities.glsl"
#include "buffer.glsl"

layout (local_size_x = 16, local_size_y = 16) in;

layout (set = 0, binding = 0) uniform texture2D u_depth_in;
layout (set = 0, binding = 1) restrict writeonly uniform image2D u_depth_out;

layout (scalar, push_constant) restrict readonly uniform u_push_constant_block {
    restrict readonly b_view_block u_view_ptr;
};

void main() {
    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, imageSize(u_depth_out)))) {
        return;
    }
    const view_t view = u_view_ptr.data[IRIS_MAIN_VIEW_INDEX];
    const ivec2 position = ivec2(gl_GlobalInvocationID.xy);
    const float near_plane = view.near_far.x;
    const float far_plane = view.near_far.y;
    const float depth = texelFetch(u_depth_in, position, 0).r;
    if (depth == 0) {
        imageStore(u_depth_out, position, vec4(1, 0, 0, 0));
        return;
    }
    const float linear_depth = min(near_plane / depth, far_plane);
    const float rescaled_depth = saturate((linear_depth - near_plane) / (far_plane - near_plane));
    imageStore(u_depth_out, position, vec4(rescaled_depth));
}
