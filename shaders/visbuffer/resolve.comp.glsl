#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "visbuffer/common.glsl"
#include "vsm/common.glsl"
#include "utilities.glsl"
#include "buffer.glsl"

layout (local_size_x = 16, local_size_y = 16) in;

layout (set = 0, binding = 0) uniform texture2D u_depth;
layout (r32ui, set = 0, binding = 1) restrict readonly uniform uimage2D u_vsm;
layout (r32ui, set = 0, binding = 2) restrict readonly uniform uimage2D u_visbuffer;
layout (rgba32f, set = 0, binding = 3) restrict writeonly uniform image2D u_output;

layout (set = 0, binding = 4) uniform sampler u_sampler;
layout (set = 0, binding = 5) uniform texture2D[] u_textures;

layout (scalar, push_constant) restrict uniform u_push_constants_block {
    restrict /*readonly*/ b_view_block u_view_ptr;
    restrict readonly b_meshlet_instance_block u_meshlet_instance_ptr;
    restrict readonly b_meshlet_block u_meshlet_ptr;
    restrict readonly b_transform_block u_transform_ptr;
    restrict readonly b_vertex_block u_vertex_ptr;
    restrict readonly b_index_block u_index_ptr;
    restrict readonly b_primitive_block u_primitive_ptr;

    restrict readonly b_material_block u_material_ptr;
    restrict readonly b_directional_light_block u_dir_light_ptr;

    restrict /*readonly*/ b_vsm_globals_block u_vsm_globals_ptr;
    restrict readonly b_vsm_virtual_page_table_block u_virt_page_table_ptr;
};

const vec3[] _debug_clipmap_colors = vec3[](
    vec3(1.0, 0.5, 0.5),
    vec3(0.5, 1.0, 0.5),
    vec3(0.5, 0.5, 1.0),
    vec3(1.0, 1.0, 0.5),
    vec3(1.0, 0.5, 1.0),
    vec3(0.5, 1.0, 1.0),
    vec3(1.0, 0.5, 0.0),
    vec3(0.5, 1.0, 0.0),
    vec3(0.0, 0.5, 1.0),
    vec3(1.0, 0.0, 0.5),
    vec3(0.0, 1.0, 0.5),
    vec3(0.5, 0.0, 1.0),
    vec3(1.0, 0.5, 0.5),
    vec3(0.5, 1.0, 0.5),
    vec3(0.5, 0.5, 1.0),
    vec3(1.0, 1.0, 0.5)
);

uint[3] load_primitive_indices(in uint offset, in uint index) {
    return uint[3](
        u_primitive_ptr.data[offset + index * 3 + 0],
        u_primitive_ptr.data[offset + index * 3 + 1],
        u_primitive_ptr.data[offset + index * 3 + 2]
    );
}

vec3[3] load_vertex_position(in meshlet_t meshlet, in uint[3] primitive) {
    return vec3[3](
        vec3(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[0]]].position),
        vec3(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[1]]].position),
        vec3(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[2]]].position)
    );
}

vec3[3] load_vertex_normal(in meshlet_t meshlet, in uint[3] primitive) {
    return vec3[3](
        vec3(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[0]]].normal),
        vec3(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[1]]].normal),
        vec3(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[2]]].normal)
    );
}

vec2[3] load_vertex_uv(in meshlet_t meshlet, in uint[3] primitive) {
    return vec2[3](
        vec2(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[0]]].uv),
        vec2(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[1]]].uv),
        vec2(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[2]]].uv)
    );
}

vec4[3] load_vertex_tangent(in meshlet_t meshlet, in uint[3] primitive) {
    return vec4[3](
        vec4(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[0]]].tangent),
        vec4(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[1]]].tangent),
        vec4(u_vertex_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + primitive[2]]].tangent)
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
    const mat4 jittered_proj_view = main_view.jittered_projection * main_view.view;
    const mat4 inv_jittered_proj_view = inverse(jittered_proj_view);
    const mat4 transform = u_transform_ptr.data[instance_id].model;
    const mat3 normal_transform = mat3(transpose(inverse(transform)));
    const vec2 view_uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(size);

    const float v_depth = texelFetch(u_depth, ivec2(gl_GlobalInvocationID.xy), 0).x;
    const uint[] primitive_indices = load_primitive_indices(meshlet.primitive_offset, primitive_id);
    const vec3[] vertex_positions = load_vertex_position(meshlet, primitive_indices);
    const vec3[] vertex_normals = load_vertex_normal(meshlet, primitive_indices);
    const vec2[] vertex_uvs = load_vertex_uv(meshlet, primitive_indices);
    const vec4[] vertex_tangents = load_vertex_tangent(meshlet, primitive_indices);

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

    const virtual_page_info_t virtual_page = virtual_page_info_from_depth(
        u_view_ptr,
        u_vsm_globals_ptr,
        vec2(size),
        inv_jittered_proj_view,
        uvec2(gl_GlobalInvocationID.xy),
        v_depth
    );
    const uint clipmap_level = virtual_page.clipmap_level;

    float shadow_factor = 1.0;
    {
        const uvec2 virtual_page_position = clamp(virtual_page.stable_position.xy, uvec2(0), uvec2(IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE - 1));
        const uint virtual_page_index = virtual_page_position.x + virtual_page_position.y * IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE;
        const uint virtual_page_entry = u_virt_page_table_ptr.data[virtual_page_index +  clipmap_level * IRIS_VSM_VIRTUAL_PAGE_COUNT];
        const vec2 virtual_page_uv = IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE * mod(virtual_page.stable_uv, 1.0 / IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE);
        if (is_virtual_page_backed(virtual_page_entry)) {
            const uvec2 physical_page_corner = calculate_physical_page_texel_corner(virtual_page_entry);
            const uvec2 physical_texel = physical_page_corner + uvec2(virtual_page_uv * IRIS_VSM_PHYSICAL_PAGE_SIZE);
            const float depth = uintBitsToFloat(imageLoad(u_vsm, ivec2(physical_texel)).r);
            shadow_factor = depth < virtual_page.depth ? 0.0 : 1.0;
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
        color += light_ambient * base_color;
        color += light_intensity * (light_diffuse + light_specular) * base_color * shadow_factor;
    }
    color *= _debug_clipmap_colors[clipmap_level];
    const vec2 virtual_page_uv = IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE * mod(virtual_page.stable_uv, 1.0 / IRIS_VSM_VIRTUAL_PAGE_ROW_SIZE);
    if (
        virtual_page_uv.x < 0.01 ||
        virtual_page_uv.y < 0.01 ||
        virtual_page_uv.x > 0.99 ||
        virtual_page_uv.y > 0.99
    ) {
        color = vec3(1.0, 0.0125, 0.0125);
    }
    imageStore(u_output, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1.0));
}