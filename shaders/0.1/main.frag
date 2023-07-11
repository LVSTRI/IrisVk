#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_shader_image_int64 : enable
#extension GL_EXT_mesh_shader : enable

layout (location = 0) in i_vertex_data_block {
    flat uint i_meshlet_id;
};

layout (r64ui, set = 0, binding = 1) restrict uniform u64image2D u_visbuffer;

void main() {
    const uint64_t depth = uint64_t(floatBitsToUint(gl_FragCoord.z));
    const uint64_t payload =
        (depth << 34) |
        (uint64_t(i_meshlet_id) << 7) |
        (uint64_t(gl_PrimitiveID));
    imageAtomicMax(u_visbuffer, ivec2(gl_FragCoord.xy), payload);
}
