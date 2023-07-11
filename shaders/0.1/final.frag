#version 460
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
//#extension GL_EXT_debug_printf : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_shader_image_int64 : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

struct partial_derivatives_t {
    vec3 lambda;
    vec3 ddx;
    vec3 ddy;
};

struct uv_grad_t {
    vec2 uv;
    vec2 ddx;
    vec2 ddy;
};

layout (location = 0) in vec2 i_uv;

layout (location = 0) out vec4 o_pixel;

layout (set = 0, binding = 0) uniform u_camera_block {
    camera_data_t data;
} u_camera;

layout (r64ui, set = 0, binding = 1) restrict readonly uniform u64image2D u_visbuffer;

layout (set = 0, binding = 2) uniform sampler2D[] u_textures;

layout (scalar, buffer_reference) restrict readonly buffer b_meshlet_buffer {
    meshlet_glsl_t[] data;
};

layout (scalar, buffer_reference) restrict readonly buffer b_meshlet_instance_buffer {
    meshlet_instance_t[] data;
};

layout (scalar, buffer_reference) restrict readonly buffer b_vertex_buffer {
    vertex_format_t[] data;
};

layout (scalar, buffer_reference) restrict readonly buffer b_index_buffer {
    uint[] data;
};

layout (scalar, buffer_reference) restrict readonly buffer b_primitive_buffer {
    uint8_t[] data;
};

layout (scalar, buffer_reference) restrict readonly buffer b_transform_buffer {
    mat4[] data;
};

layout (scalar, buffer_reference) restrict readonly buffer b_cluster_classification {
    uint8_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_meshlet_size {
    uint[] data;
};

layout (push_constant) uniform pc_address_block {
    b_meshlet_buffer meshlet_ptr;
    b_meshlet_instance_buffer meshlet_instance_ptr;
    b_vertex_buffer vertex_ptr;
    b_index_buffer index_ptr;
    b_primitive_buffer primitive_ptr;
    b_transform_buffer transform_ptr;
    uint view_mode;
};

partial_derivatives_t compute_derivatives(in vec4[3] clip_position, in vec2 ndc_uv, in vec2 resolution) {
    partial_derivatives_t result;
    const vec3 inv_w = 1.0 / vec3(clip_position[0].w, clip_position[1].w, clip_position[2].w);
    const vec2 ndc_0 = clip_position[0].xy * inv_w[0];
    const vec2 ndc_1 = clip_position[1].xy * inv_w[1];
    const vec2 ndc_2 = clip_position[2].xy * inv_w[2];

    const float inv_det = 1.0 / determinant(mat2(ndc_2 - ndc_1, ndc_0 - ndc_1));
    result.ddx = vec3(ndc_1.y - ndc_2.y, ndc_2.y - ndc_0.y, ndc_0.y - ndc_1.y) * inv_det * inv_w;
    result.ddy = vec3(ndc_2.x - ndc_1.x, ndc_0.x - ndc_2.x, ndc_1.x - ndc_0.x) * inv_det * inv_w;

    float ddx_sum = dot(result.ddx, vec3(1.0));
    float ddy_sum = dot(result.ddy, vec3(1.0));

    const vec2 delta_v = ndc_uv - ndc_0;
    const float interp_inv_w = inv_w.x + delta_v.x * ddx_sum + delta_v.y * ddy_sum;
    const float interp_w = 1.0 / interp_inv_w;

    result.lambda = vec3(
        interp_w * (delta_v.x * result.ddx.x + delta_v.y * result.ddy.x + inv_w.x),
        interp_w * (delta_v.x * result.ddx.y + delta_v.y * result.ddy.y),
        interp_w * (delta_v.x * result.ddx.z + delta_v.y * result.ddy.z));

    result.ddx *= 2.0 / resolution.x;
    result.ddy *= -2.0 / resolution.y;
    ddx_sum *= 2.0 / resolution.x;
    ddy_sum *= -2.0 / resolution.y;

    const float interp_w_ddx = 1.0 / (interp_inv_w.x + ddx_sum);
    const float interp_w_ddy = 1.0 / (interp_inv_w.x + ddy_sum);

    result.ddx = interp_w_ddx * (result.lambda * interp_inv_w + result.ddx) - result.lambda;
    result.ddy = interp_w_ddy * (result.lambda * interp_inv_w + result.ddy) - result.lambda;
    return result;
}

vec3 interpolate_attributes(in partial_derivatives_t derivatives, in float[3] attributes) {
    const vec3 v = vec3(attributes[0], attributes[1], attributes[2]);
    return vec3(
        dot(v, derivatives.lambda),
        dot(v, derivatives.ddx),
        dot(v, derivatives.ddy));
}

vec3 interpolate_attributes(in partial_derivatives_t derivatives, in vec3[3] attributes) {
    return
        attributes[0] * derivatives.lambda.x +
        attributes[1] * derivatives.lambda.y +
        attributes[2] * derivatives.lambda.z;
}

vec4 interpolate_attributes(in partial_derivatives_t derivatives, in vec4[3] attributes) {
    return
        attributes[0] * derivatives.lambda.x +
        attributes[1] * derivatives.lambda.y +
        attributes[2] * derivatives.lambda.z;
}

void main() {
    const uvec2 resolution = imageSize(u_visbuffer).xy;
    const uint64_t payload = imageLoad(u_visbuffer, ivec2(gl_FragCoord.xy)).x;
    const uint depth_bits = uint((payload >> 34) & 0x3fffffff);
    if (depth_bits == 0) {
        discard;
    }
    const float depth = uintBitsToFloat(depth_bits);
    const uint meshlet_instance_id = uint((payload >> 7) & 0x07ffffff);
    const uint primitive_id = uint(payload & 0x7f);

    // fetch meshlet data
    const uint meshlet_id = meshlet_instance_ptr.data[meshlet_instance_id].meshlet_id;
    const uint instance_id = meshlet_instance_ptr.data[meshlet_instance_id].instance_id;

    const meshlet_glsl_t meshlet = meshlet_ptr.data[meshlet_id];
    const uint vertex_offset = meshlet.vertex_offset;
    const uint index_offset = meshlet.index_offset;
    const uint primitive_offset = meshlet.primitive_offset;
    const uint index_count = meshlet.index_count;
    const uint primitive_count = meshlet.primitive_count;
    const meshlet_material_glsl_t material = meshlet.material;

    // fetch vertex data
    const restrict uint[] primitive_index = uint[](
        uint(primitive_ptr.data[primitive_offset + primitive_id * 3 + 0]),
        uint(primitive_ptr.data[primitive_offset + primitive_id * 3 + 1]),
        uint(primitive_ptr.data[primitive_offset + primitive_id * 3 + 2]));
    const restrict vertex_format_t[] vertices = vertex_format_t[](
        vertex_ptr.data[vertex_offset + index_ptr.data[index_offset + primitive_index[0]]],
        vertex_ptr.data[vertex_offset + index_ptr.data[index_offset + primitive_index[1]]],
        vertex_ptr.data[vertex_offset + index_ptr.data[index_offset + primitive_index[2]]]);
    const restrict mat4 transform = transform_ptr.data[instance_id];

    const vec4[] clip_position = vec4[](
        u_camera.data.pv * transform * vec4(vec3_from_float(vertices[0].position), 1.0),
        u_camera.data.pv * transform * vec4(vec3_from_float(vertices[1].position), 1.0),
        u_camera.data.pv * transform * vec4(vec3_from_float(vertices[2].position), 1.0));
    const vec3 position = unproject_depth(u_camera.data.pv, depth, i_uv);

    const partial_derivatives_t derivatives = compute_derivatives(clip_position, i_uv * 2.0 - 1.0, vec2(resolution));

    const vec3[] normals = vec3[](
        vec3_from_float(vertices[0].normal),
        vec3_from_float(vertices[1].normal),
        vec3_from_float(vertices[2].normal));
    const vec3 normal = normalize(mat3(transpose(inverse(transform))) * normalize(interpolate_attributes(derivatives, normals)));

    const vec2[] uvs = vec2[](
        vec2_from_float(vertices[0].uv),
        vec2_from_float(vertices[1].uv),
        vec2_from_float(vertices[2].uv));
    const vec3[] interp_uv = vec3[](
        interpolate_attributes(derivatives, float[](uvs[0].x, uvs[1].x, uvs[2].x)),
        interpolate_attributes(derivatives, float[](uvs[0].y, uvs[1].y, uvs[2].y)));
    const uv_grad_t uv_grad = uv_grad_t(
        vec2(interp_uv[0].x, interp_uv[1].x),
        vec2(interp_uv[0].y, interp_uv[1].y),
        vec2(interp_uv[0].z, interp_uv[1].z));

    switch (view_mode) {
        case 0: {
            if (material.base_color_texture != -1) {
                o_pixel = vec4(textureGrad(u_textures[material.base_color_texture], uv_grad.uv, uv_grad.ddx, uv_grad.ddy).rgb, 1.0);
            } else {
                o_pixel = vec4(vec3(0.0), 1.0);
            }
            break;
        }

        case 1: {
            o_pixel = vec4(hsv_to_rgb(vec3(float(meshlet_instance_id) * M_GOLDEN_CONJ, 0.875, 0.85)), 1.0);
            break;
        }

        case 2: {
            o_pixel = vec4(hsv_to_rgb(vec3(float(primitive_id) * M_GOLDEN_CONJ, 0.875, 0.85)), 1.0);
            break;
        }
    }
    //o_pixel = vec4(normal * 0.5 + 0.5, 1.0);
}
