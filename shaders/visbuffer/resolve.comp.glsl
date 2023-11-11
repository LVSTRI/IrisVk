#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "shadow/common.glsl"
#include "visbuffer/common.glsl"
#include "utilities.glsl"
#include "buffer.glsl"

layout (local_size_x = 16, local_size_y = 16) in;

layout (set = 0, binding = 0) uniform texture2D u_depth;
layout (r32ui, set = 0, binding = 1) restrict readonly uniform uimage2D u_visbuffer;
layout (rgba32f, set = 0, binding = 2) restrict writeonly uniform image2D u_output;
layout (set = 0, binding = 3) uniform sampler2DArrayShadow u_shadow;

layout (set = 0, binding = 4) uniform sampler u_sampler;
layout (set = 0, binding = 5) uniform texture2D[] u_textures;

layout (scalar, push_constant) restrict readonly uniform u_push_constants_block {
    restrict readonly b_view_block u_view_ptr;
    restrict readonly b_meshlet_instance_block u_meshlet_instance_ptr;
    restrict readonly b_meshlet_block u_meshlet_ptr;
    restrict readonly b_transform_block u_transform_ptr;
    restrict readonly b_position_block u_position_ptr;
    restrict readonly b_vertex_block u_vertex_ptr;
    restrict readonly b_index_block u_index_ptr;
    restrict readonly b_primitive_block u_primitive_ptr;

    restrict readonly b_material_block u_material_ptr;
    restrict readonly b_directional_light_block u_dir_light_ptr;
    restrict readonly b_shadow_cascade_block u_shadow_cascade_ptr;
};

uint[3] load_primitive_indices(in uint offset, in uint index) {
    return uint[3](
        u_primitive_ptr.data[offset + index * 3 + 0],
        u_primitive_ptr.data[offset + index * 3 + 1],
        u_primitive_ptr.data[offset + index * 3 + 2]
    );
}

vec3[3] load_vertex_position(in meshlet_t meshlet, in uint[3] primitive) {
    return vec3[3](
        vec3(u_position_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[0]]]),
        vec3(u_position_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[1]]]),
        vec3(u_position_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[2]]])
    );
}

vec3 load_base_color(in material_t material, in gradient_vec2_t uv_grad) {
    vec3 color = material.base_color_factor;
    if (material.base_color_texture != uint(-1)) {
        color *= textureGrad(
            sampler2D(u_textures[material.base_color_texture], u_sampler),
            uv_grad.lambda,
            uv_grad.ddx,
            uv_grad.ddy
        ).rgb;
    }
    return color;
}

vec3 load_normal(in material_t material, in gradient_vec2_t uv_grad, in mat3 tbn, in vec3 normal) {
    if (material.normal_texture != uint(-1)) {
        const vec2 sampled_normal = textureGrad(
            sampler2D(u_textures[material.normal_texture], u_sampler),
            uv_grad.lambda,
            uv_grad.ddx,
            uv_grad.ddy
        ).rg * 2.0 - 1.0;
        const float z = sqrt(max(1.0 - dot(sampled_normal, sampled_normal), 0.0));
        return normalize(tbn * vec3(sampled_normal, z));
    }
    return normal;
}

mat3 make_tbn(in mat3 transform, in vec3 normal, in vec4 tangent) {
    const vec3 bitangent = cross(normal, tangent.xyz) * tangent.w;
    const vec3 T = normalize(transform * tangent.xyz);
    const vec3 B = normalize(transform * bitangent);
    const vec3 N = normalize(transform * normal);
    return mat3(T, B, N);
}

float sample_shadow(
    in directional_light_t light,
    in shadow_cascade_t cascade,
    in vec3 shadow_position,
    in vec3 normal,
    in float distance,
    in uint layer
) {
    shadow_position += cascade.offset.xyz;
    shadow_position *= cascade.scale.xyz;
    const float receiver_depth = shadow_position.z;
    const uint sample_count = 64;
    const float kernel_radius = 4.0;
    const float texel_size = 1.0 / float(IRIS_SHADOW_CASCADE_RESOLUTION);
    float shadow_factor = 0.0;
    iris_random_set_seed(gl_GlobalInvocationID.x * gl_WorkGroupSize.x + gl_GlobalInvocationID.y);
    for (uint i = 0; i < sample_count; ++i) {
        const vec2 noise_texel = vec2(
            sample_blue_noise_simple(vec2((gl_GlobalInvocationID.xy * iris_random()) % 1024)),
            sample_blue_noise_simple(vec2((gl_GlobalInvocationID.xy * iris_random()) % 1024))
        );
        const vec2 xi = fract(sample_hammersley(i, sample_count) + noise_texel);
        const float r = sqrt(xi.x);
        const float theta = xi.y * 2.0 * M_PI;
        const vec2 offset = vec2(r * cos(theta), r * sin(theta));
        const vec2 sample_position = shadow_position.xy + offset * kernel_radius * texel_size;
        shadow_factor += texture(u_shadow, vec4(sample_position, layer, receiver_depth)).r;
    }
    return shadow_factor / float(sample_count);
}

float calculate_shadow(
    in directional_light_t light,
    in shadow_cascade_t cascade,
    in vec3 position,
    in vec3 normal,
    in float distance,
    in uint layer
) {
    const vec3 shadow_position = vec3(cascade.global * vec4(position, 1.0));
    return sample_shadow(light, cascade, shadow_position, normal, distance, layer);
}

void main() {
    const ivec2 size = imageSize(u_visbuffer);
    if (any(greaterThanEqual(gl_GlobalInvocationID.xy, size))) {
        return;
    }
    const uint payload = imageLoad(u_visbuffer, ivec2(gl_GlobalInvocationID.xy)).x;
    if (payload == uint(-1)) {
        imageStore(u_output, ivec2(gl_GlobalInvocationID.xy), vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    const uint meshlet_instance_id = (payload >> CLUSTER_PRIMITIVE_ID_BITS) & CLUSTER_ID_MASK;
    const uint primitive_id = payload & CLUSTER_PRIMITIVE_ID_MASK;
    const uint meshlet_id = u_meshlet_instance_ptr.data[meshlet_instance_id].meshlet_id;
    const uint instance_id = u_meshlet_instance_ptr.data[meshlet_instance_id].instance_id;
    const uint material_id = u_meshlet_instance_ptr.data[meshlet_instance_id].material_id;
    const meshlet_t meshlet = u_meshlet_ptr.data[meshlet_id];
    const view_t main_view = u_view_ptr.data[IRIS_MAIN_VIEW_INDEX];
    const uint[] primitive_indices = load_primitive_indices(meshlet.primitive_offset, primitive_id);
    const vertex_format_t[] vertex = vertex_format_t[](
        u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive_indices[0]]],
        u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive_indices[1]]],
        u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive_indices[2]]]
    );
    const mat4 jittered_proj_view = main_view.jittered_projection * main_view.view;
    const mat4 inv_jittered_proj_view = inverse(jittered_proj_view);
    const mat4 transform = u_transform_ptr.data[instance_id].model;
    const mat3 normal_transform = mat3(transpose(inverse(transform)));
    const vec2 view_uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(size);

    const float v_depth = texelFetch(u_depth, ivec2(gl_GlobalInvocationID.xy), 0).x;
    const vec3[] vertex_positions = load_vertex_position(meshlet, primitive_indices);
    const vec3[] vertex_normals = vec3[](
        vertex[0].normal,
        vertex[1].normal,
        vertex[2].normal
    );
    const vec2[] vertex_uvs = vec2[](
        vertex[0].uv,
        vertex[1].uv,
        vertex[2].uv
    );
    const vec4[] vertex_tangents = vec4[](
        vertex[0].tangent,
        vertex[1].tangent,
        vertex[2].tangent
    );
    const vec3[] world_position = vec3[](
        vec3(transform * vec4(vertex_positions[0], 1.0)),
        vec3(transform * vec4(vertex_positions[1], 1.0)),
        vec3(transform * vec4(vertex_positions[2], 1.0))
    );
    const vec4[] clip_position = vec4[](
        jittered_proj_view * vec4(world_position[0], 1.0),
        jittered_proj_view * vec4(world_position[1], 1.0),
        jittered_proj_view * vec4(world_position[2], 1.0)
    );
    const vec3 position = uv_to_world(inv_jittered_proj_view, view_uv, v_depth);
    const vec3 view_direction = normalize(main_view.eye_position.xyz - position);

    const partial_derivatives_t derivatives = compute_derivatives(clip_position, view_uv * 2.0 - 1.0, vec2(size));
    const gradient_vec2_t uv_grad = make_gradient_vec2(derivatives, vertex_uvs);

    const vec3 base_normal = normalize(interpolate(derivatives, vec3[](
        vertex_normals[0],
        vertex_normals[1],
        vertex_normals[2]
    )));

    const vec4 base_tangent = normalize(interpolate(derivatives, vec4[](
        vertex_tangents[0],
        vertex_tangents[1],
        vertex_tangents[2]
    )));
    const float position_distance = (main_view.proj_view * vec4(position, 1.0)).w;

    uint shadow_cascade_layer = IRIS_SHADOW_CASCADE_COUNT - 1;
    shadow_cascade_t shadow_cascade = u_shadow_cascade_ptr.data[shadow_cascade_layer];
    {
        for (uint i = 0; i < IRIS_SHADOW_CASCADE_COUNT; ++i) {
            shadow_cascade = u_shadow_cascade_ptr.data[i];
            if (position_distance < shadow_cascade.offset.w) {
                shadow_cascade_layer = i;
                break;
            }
        }
    }

    const float light_ambient = 0.025;
    vec3 color = vec3(0.0);
    {
        vec3 base_color = vec3(1.0);
        vec3 normal = normalize(normal_transform * base_normal);
        if (material_id != uint(-1)) {
            const material_t material = u_material_ptr.data[material_id];
            const mat3 tbn = make_tbn(normal_transform, base_normal, base_tangent);
            base_color = load_base_color(material, uv_grad);
            normal = load_normal(material, uv_grad, tbn, normal);
        }
        const directional_light_t sun_dir_light = u_dir_light_ptr.data[0];
        const vec3 light_dir = sun_dir_light.direction;
        const float light_intensity = sun_dir_light.intensity;
        const float light_diffuse = saturate(dot(normal, light_dir));
        const float light_specular = pow(saturate(dot(reflect(-light_dir, normal), view_direction)), 64.0);
        const float shadow = calculate_shadow(
            sun_dir_light,
            shadow_cascade,
            position,
            normal,
            position_distance,
            shadow_cascade_layer
        );
        color += light_ambient * base_color;
        color += light_intensity * (light_diffuse + light_specular) * base_color * shadow;
    }

    /*switch (shadow_cascade_layer) {
        case 0: color *= vec3(1.0, 0.25, 0.25); break;
        case 1: color *= vec3(0.25, 1.0, 0.25); break;
        case 2: color *= vec3(0.25, 0.25, 1.0); break;
        case 3: color *= vec3(1.0, 1.0, 0.25); break;
    }*/
    imageStore(u_output, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1.0));
}
