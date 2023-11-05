#version 460
#extension GL_EXT_scalar_block_layout : enable

#include "intrinsics.glsl"

#define IRIS_TEXTURE_TYPE_2D_SFLOAT 0
#define IRIS_TEXTURE_TYPE_2D_SINT 1
#define IRIS_TEXTURE_TYPE_2D_UINT 2
#define IRIS_TEXTURE_TYPE_2D_ARRAY_SFLOAT 3
#define IRIS_TEXTURE_TYPE_2D_ARRAY_SINT 4
#define IRIS_TEXTURE_TYPE_2D_ARRAY_UINT 5

#define sampler_partially_bound decorate_with_string("update_after_bind|partially_bound")

layout (local_size_x = 16, local_size_y = 16) in;

layout (set = 0, binding = IRIS_TEXTURE_TYPE_2D_SFLOAT) sampler_partially_bound uniform sampler2D u_texture_2d_sfloat;
layout (set = 0, binding = IRIS_TEXTURE_TYPE_2D_SINT) sampler_partially_bound uniform isampler2D u_texture_2d_sint;
layout (set = 0, binding = IRIS_TEXTURE_TYPE_2D_UINT) sampler_partially_bound uniform usampler2D u_texture_2d_uint;
layout (set = 0, binding = IRIS_TEXTURE_TYPE_2D_ARRAY_SFLOAT) sampler_partially_bound uniform sampler2DArray u_texture_2d_array_sfloat;
layout (set = 0, binding = IRIS_TEXTURE_TYPE_2D_ARRAY_SINT) sampler_partially_bound uniform isampler2DArray u_texture_2d_array_sint;
layout (set = 0, binding = IRIS_TEXTURE_TYPE_2D_ARRAY_UINT) sampler_partially_bound uniform usampler2DArray u_texture_2d_array_uint;

layout (rgba8, set = 0, binding = 6) restrict writeonly uniform image2D u_image;

layout (push_constant) restrict uniform u_push_constant_block {
    uint u_texture_type;
    uint u_texture_layer;
    uint u_texture_level;
};

void main() {
    const ivec2 position = ivec2(gl_GlobalInvocationID.xy);
    const ivec2 size = imageSize(u_image);
    if (any(greaterThanEqual(position, size))) {
        return;
    }
    const vec2 uv = (vec2(position) + 0.5) / size;
    switch (u_texture_type) {
        case IRIS_TEXTURE_TYPE_2D_SFLOAT: {
            const vec3 color = textureLod(u_texture_2d_sfloat, uv, u_texture_level).rgb;
            imageStore(u_image, position, vec4(color, 1.0));
        } break;
        case IRIS_TEXTURE_TYPE_2D_SINT: {
            const ivec3 color = textureLod(u_texture_2d_sint, uv, u_texture_level).rgb;
            imageStore(u_image, position, vec4(color, 1.0));
        } break;
        case IRIS_TEXTURE_TYPE_2D_UINT: {
            const uvec3 color = textureLod(u_texture_2d_uint, uv, u_texture_level).rgb;
            imageStore(u_image, position, vec4(color, 1.0));
        } break;
        case IRIS_TEXTURE_TYPE_2D_ARRAY_SFLOAT: {
            const vec3 color = textureLod(u_texture_2d_array_sfloat, vec3(uv, u_texture_layer), u_texture_level).rgb;
            imageStore(u_image, position, vec4(color, 1.0));
        } break;
        case IRIS_TEXTURE_TYPE_2D_ARRAY_SINT: {
            const ivec3 color = textureLod(u_texture_2d_array_sint, vec3(uv, u_texture_layer), u_texture_level).rgb;
            imageStore(u_image, position, vec4(color, 1.0));
        } break;
        case IRIS_TEXTURE_TYPE_2D_ARRAY_UINT: {
            const uvec3 color = textureLod(u_texture_2d_array_uint, vec3(uv, u_texture_layer), u_texture_level).rgb;
            imageStore(u_image, position, vec4(color, 1.0));
        } break;
    }
}
