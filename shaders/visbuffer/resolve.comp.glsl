#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_KHR_shader_subgroup : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "visbuffer/common.glsl"
#include "utilities.glsl"

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout (r32ui, set = 0, binding = 0) uniform restrict readonly uimage2D u_visbuffer;
layout (rgba32f, set = 0, binding = 1) uniform restrict writeonly image2D u_output;

void main() {
    const ivec2 size = imageSize(u_visbuffer);
    if (gl_GlobalInvocationID.x >= size.x || gl_GlobalInvocationID.y >= size.y) {
        return;
    }
    const uint payload = imageLoad(u_visbuffer, ivec2(gl_GlobalInvocationID.xy)).x;
    if (payload == uint(-1)) {
        imageStore(u_output, ivec2(gl_GlobalInvocationID.xy), vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    const uint meshlet_instance_id = (payload >> CLUSTER_PRIMITIVE_ID_BITS) & CLUSTER_ID_MASK;
    const uint primitive_id = payload & CLUSTER_PRIMITIVE_ID_MASK;

    vec3 color = hsv_to_rgb(vec3(float(meshlet_instance_id) * M_GOLDEN_CONJ, 0.85, 0.875));
    imageStore(u_output, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1.0));
}