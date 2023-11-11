#version 460
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_debug_printf : enable
#include "shadow/common.glsl"
#include "buffer.glsl"
#include "utilities.glsl"

layout (local_size_x = IRIS_SHADOW_CASCADE_COUNT) in;

layout (rg32f, set = 0, binding = 0) restrict readonly uniform image2D u_reduced_depth;

layout (scalar, push_constant) restrict uniform u_push_constant_block {
    restrict readonly b_view_block u_view_ptr;
    restrict readonly b_directional_light_block u_directional_light_ptr;
    restrict writeonly b_shadow_cascade_block u_shadow_cascade_ptr;
};

const mat4 g_uv_scale_matrix = mat4(
    vec4(0.5, 0.0, 0.0, 0.0),
    vec4(0.0, 0.5, 0.0, 0.0),
    vec4(0.0, 0.0, 1.0, 0.0),
    vec4(0.5, 0.5, 0.0, 1.0)
);

mat4 mat4_identity() {
    mat4 m = mat4(0.0);
    m[0][0] = 1.0;
    m[1][1] = 1.0;
    m[2][2] = 1.0;
    m[3][3] = 1.0;
    return m;
}

mat4 mat4_make_ortho(in float left, in float right, in float bottom, in float top, in float near, in float far) {
    mat4 m = mat4_identity();
    m[0][0] = 2.0 / (right - left);
    m[1][1] = -2.0 / (top - bottom);
    m[2][2] = -1.0 / (far - near);
    m[3][0] = -(right + left) / (right - left);
    m[3][1] = -(top + bottom) / (top - bottom);
    m[3][2] = -near / (far - near);
    return m;
}

mat4 mat4_make_view(in vec3 eye, in vec3 center, in vec3 up) {
    mat4 m = mat4_identity();
    vec3 f = normalize(center - eye);
    vec3 s = normalize(cross(f, up));
    vec3 u = cross(s, f);
    m[0][0] =  s.x;
    m[1][0] =  s.y;
    m[2][0] =  s.z;
    m[0][1] =  u.x;
    m[1][1] =  u.y;
    m[2][1] =  u.z;
    m[0][2] = -f.x;
    m[1][2] = -f.y;
    m[2][2] = -f.z;
    m[3][0] = -dot(s, eye);
    m[3][1] = -dot(u, eye);
    m[3][2] =  dot(f, eye);
    return m;
}

vec4 make_plane_from_points(in vec3 a, in vec3 b, in vec3 c) {
    const vec3 n = normalize(cross(c - a, b - a));
    return vec4(n, dot(n, a));
}

mat4 make_main_camera_finite_proj_view(in view_t view) {
    return view.finite_projection * view.view;
}

mat4 make_main_camera_global_proj_view(in view_t view, in vec3 light_dir) {
    const mat4 inv_proj_view = inverse(make_main_camera_finite_proj_view(view));
    vec3[] frustum_corners = vec3[](
        vec3(-1.0, -1.0, 0.0),
        vec3( 1.0, -1.0, 0.0),
        vec3(-1.0,  1.0, 0.0),
        vec3( 1.0,  1.0, 0.0),
        vec3(-1.0, -1.0, 1.0),
        vec3( 1.0, -1.0, 1.0),
        vec3(-1.0,  1.0, 1.0),
        vec3( 1.0,  1.0, 1.0));
    vec3 frustum_center = vec3(0.0);
    for (uint i = 0; i < 8; ++i) {
        const vec4 corner = inv_proj_view * vec4(frustum_corners[i], 1.0);
        frustum_corners[i] = corner.xyz / corner.w;
        frustum_center += frustum_corners[i];
    }
    frustum_center /= 8.0;

    vec3 min_frustum_extension = vec3(FLT_MAX);
    vec3 max_frustum_extension = vec3(-FLT_MAX);
    for (uint i = 0; i < 8; ++i) {
        min_frustum_extension = vec3(min(min_frustum_extension.x, frustum_corners[i].x));
        min_frustum_extension = vec3(min(min_frustum_extension.y, frustum_corners[i].y));
        min_frustum_extension = vec3(min(min_frustum_extension.z, frustum_corners[i].z));
        max_frustum_extension = vec3(max(max_frustum_extension.x, frustum_corners[i].x));
        max_frustum_extension = vec3(max(max_frustum_extension.y, frustum_corners[i].y));
        max_frustum_extension = vec3(max(max_frustum_extension.z, frustum_corners[i].z));
    }
    const mat4 global_projection = mat4_make_ortho(
        min_frustum_extension.x,
        max_frustum_extension.x,
        min_frustum_extension.y,
        max_frustum_extension.y,
        0.0,
        1.0);
    const mat4 global_view = mat4_make_view(frustum_center + light_dir * 0.5, frustum_center, vec3(0.0, 1.0, 0.0));
    return g_uv_scale_matrix * (global_projection * global_view);
}

void main() {
    const view_t main_view = u_view_ptr.data[IRIS_MAIN_VIEW_INDEX];
    const directional_light_t light = u_directional_light_ptr.data[0];

    const float main_camera_near_clip = main_view.near_far.x;
    const float main_camera_far_clip = main_view.near_far.y;
    const vec2 min_max_depth = vec2(imageLoad(u_reduced_depth, ivec2(0)));
    const float min_depth = 0.0;
    const float max_depth = 1.0;

    float[] cascade_split = float[](0, 0, 0, 0);
    {
        const float lambda = 0.9;
        const float clip_range = main_camera_far_clip - main_camera_near_clip;
        const float min_z = main_camera_near_clip + min_depth * clip_range;
        const float max_z = main_camera_near_clip + max_depth * clip_range;
        const float range = max_z - min_z;
        const float ratio = max_z / min_z;

        for (uint i = 0; i < IRIS_SHADOW_CASCADE_COUNT; ++i) {
            const float p = (i + 1) / float(IRIS_SHADOW_CASCADE_COUNT);
            const float split_log = min_z * pow(abs(ratio), p);
            const float split_uniform = min_z + range * p;
            const float dist = lambda * (split_log - split_uniform) + split_uniform;
            cascade_split[i] = (dist - main_camera_near_clip) / clip_range;
        }
    }

    const uint cascade_index = gl_GlobalInvocationID.x;
    const float prev_split = cascade_index == 0 ? min_depth : cascade_split[cascade_index - 1];
    const float curr_split = cascade_split[cascade_index];
    vec3 frustum_center = vec3(0.0);
    float sphere_frustum_radius = 0.0;
    {
        vec3[] frustum_corners = vec3[](
            vec3(-1.0, -1.0, 0.0),
            vec3( 1.0, -1.0, 0.0),
            vec3(-1.0,  1.0, 0.0),
            vec3( 1.0,  1.0, 0.0),
            vec3(-1.0, -1.0, 1.0),
            vec3( 1.0, -1.0, 1.0),
            vec3(-1.0,  1.0, 1.0),
            vec3( 1.0,  1.0, 1.0));
        const mat4 inv_proj_view = inverse(make_main_camera_finite_proj_view(main_view));
        for (uint i = 0; i < 8; ++i) {
            const vec4 corner = inv_proj_view * vec4(frustum_corners[i], 1.0);
            frustum_corners[i] = corner.xyz / corner.w;
        }
        for (uint i = 0; i < 4; ++i) {
            const vec3 corner_ray = frustum_corners[i + 4] - frustum_corners[i];
            const vec3 near_corner_ray = corner_ray * prev_split;
            const vec3 far_corner_ray = corner_ray * curr_split;
            frustum_corners[i + 4] = frustum_corners[i] + far_corner_ray;
            frustum_corners[i] = frustum_corners[i] + near_corner_ray;
        }
        for (uint i = 0; i < 8; ++i) {
            frustum_center += frustum_corners[i];
        }
        frustum_center /= 8.0;

        for (uint i = 0; i < 8; ++i) {
            sphere_frustum_radius = max(sphere_frustum_radius, length(frustum_corners[i] - frustum_center));
        }
        sphere_frustum_radius = ceil(sphere_frustum_radius * 16.0) / 16.0;
    }

    const vec3 min_frustum_extension = vec3(-sphere_frustum_radius);
    const vec3 max_frustum_extension = vec3(sphere_frustum_radius);
    const vec3 shadow_cascade_extension = max_frustum_extension - min_frustum_extension;
    const vec3 shadow_camera_position = frustum_center + light.direction * -min_frustum_extension.z;

    mat4 shadow_view = mat4_make_view(shadow_camera_position, frustum_center, vec3(0.0, 1.0, 0.0));
    mat4 shadow_projection = mat4_make_ortho(
        min_frustum_extension.x,
        max_frustum_extension.x,
        min_frustum_extension.y,
        max_frustum_extension.y,
        0.0,
        shadow_cascade_extension.z);
    // stabilize cascade
    {
        const mat4 shadow_proj_view = shadow_projection * shadow_view;
        const vec3 shadow_origin = vec3(shadow_proj_view * vec4(0.0, 0.0, 0.0, 1.0));
        const vec3 shadow_origin_scaled = shadow_origin * (IRIS_SHADOW_CASCADE_RESOLUTION / 2.0);
        const vec3 shadow_rounded_origin = round(shadow_origin_scaled);
        const vec3 shadow_origin_offset = shadow_rounded_origin - shadow_origin_scaled;
        const vec3 shadow_origin_offset_scaled = shadow_origin_offset * (2.0 / IRIS_SHADOW_CASCADE_RESOLUTION);
        shadow_projection[3][0] += shadow_origin_offset_scaled.x;
        shadow_projection[3][1] += shadow_origin_offset_scaled.y;
    }
    const mat4 shadow_proj_view = shadow_projection * shadow_view;
    vec4[6] shadow_cascade_frustum_planes;
    {
        vec3[] shadow_frustum_corners = vec3[](
            vec3(-1.0, -1.0, 0.0),
            vec3( 1.0, -1.0, 0.0),
            vec3(-1.0,  1.0, 0.0),
            vec3( 1.0,  1.0, 0.0),
            vec3(-1.0, -1.0, 1.0),
            vec3( 1.0, -1.0, 1.0),
            vec3(-1.0,  1.0, 1.0),
            vec3( 1.0,  1.0, 1.0));
        for (uint i = 0; i < 8; ++i) {
            shadow_frustum_corners[i] = vec3(shadow_proj_view * vec4(shadow_frustum_corners[i], 1.0));
        }
        shadow_cascade_frustum_planes[0] = make_plane_from_points(shadow_frustum_corners[0], shadow_frustum_corners[4], shadow_frustum_corners[2]);
        shadow_cascade_frustum_planes[1] = make_plane_from_points(shadow_frustum_corners[1], shadow_frustum_corners[3], shadow_frustum_corners[5]);
        shadow_cascade_frustum_planes[2] = make_plane_from_points(shadow_frustum_corners[3], shadow_frustum_corners[2], shadow_frustum_corners[7]);
        shadow_cascade_frustum_planes[3] = make_plane_from_points(shadow_frustum_corners[1], shadow_frustum_corners[5], shadow_frustum_corners[0]);
        shadow_cascade_frustum_planes[4] = make_plane_from_points(shadow_frustum_corners[5], shadow_frustum_corners[7], shadow_frustum_corners[4]);
        shadow_cascade_frustum_planes[5] = make_plane_from_points(shadow_frustum_corners[1], shadow_frustum_corners[0], shadow_frustum_corners[3]);
    }
    const mat4 inv_shadow_proj_view = inverse(g_uv_scale_matrix * shadow_proj_view);
    const mat4 global_proj_view = make_main_camera_global_proj_view(main_view, light.direction);
    const vec3[] shadow_cascade_corners = vec3[](
        vec3(global_proj_view * vec4(vec3(inv_shadow_proj_view * vec4(0.0, 0.0, 0.0, 1.0)), 1.0)),
        vec3(global_proj_view * vec4(vec3(inv_shadow_proj_view * vec4(1.0, 1.0, 1.0, 1.0)), 1.0))
    );
    const vec3 shadow_cascade_scale = 1.0 / (shadow_cascade_corners[1] - shadow_cascade_corners[0]);
    const float shadow_clip_distance = main_camera_near_clip + curr_split * (main_camera_far_clip - main_camera_near_clip);
    u_shadow_cascade_ptr.data[cascade_index] = shadow_cascade_t(
        shadow_projection,
        shadow_view,
        shadow_proj_view,
        global_proj_view,
        shadow_cascade_frustum_planes,
        vec4(shadow_cascade_scale, 0.0),
        vec4(-shadow_cascade_corners[0], shadow_clip_distance)
    );
}
