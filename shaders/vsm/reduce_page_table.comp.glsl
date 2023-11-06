#version 460
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout (local_size_x = 16, local_size_y = 16) in;

layout (r8ui, set = 0, binding = 0) restrict readonly uniform uimage2DArray u_input;
layout (r8ui, set = 0, binding = 1) restrict writeonly uniform uimage2DArray u_output;

void main() {
    const ivec3 position = ivec3(gl_GlobalInvocationID.xyz);
    if (any(greaterThanEqual(position.xy, imageSize(u_output).xy))) {
        return;
    }

    const bvec4 active_pages = bvec4(
        bool(imageLoad(u_input, ivec3(position.xy * 2 + ivec2(0, 0), position.z)).x),
        bool(imageLoad(u_input, ivec3(position.xy * 2 + ivec2(0, 1), position.z)).x),
        bool(imageLoad(u_input, ivec3(position.xy * 2 + ivec2(1, 0), position.z)).x),
        bool(imageLoad(u_input, ivec3(position.xy * 2 + ivec2(1, 1), position.z)).x)
    );
    imageStore(u_output, position, uvec4(any(active_pages)));
}