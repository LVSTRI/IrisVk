#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_mesh_shader : enable

#define M_GOLDEN_CONJ 0.6180339887498948482045868343656

layout (early_fragment_tests) in;

layout (location = 0) in i_vertex_data_block {
    flat uint i_meshlet_id;
    flat uint i_group_id;
    vec3 i_normal;
};

layout (location = 0) out vec4 o_pixel;

vec3 hsv_to_rgb(in vec3 hsv) {
    const vec3 rgb = clamp(abs(mod(hsv.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return hsv.z * mix(vec3(1.0), rgb, hsv.y);
}

void main() {
    o_pixel = vec4(hsv_to_rgb(vec3(fract(M_GOLDEN_CONJ * i_meshlet_id), 0.875, 0.85)), 1.0);
    //o_pixel = vec4(hsv_to_rgb(vec3(fract(M_GOLDEN_CONJ * gl_PrimitiveID), 0.875, 0.85)), 1.0);
    //o_pixel = vec4(normalize(i_normal), 1.0);
}
