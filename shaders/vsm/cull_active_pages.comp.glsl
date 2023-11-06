#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "vsm/common.glsl"
#include "buffer.glsl"
#include "utilities.glsl"

layout (local_size_x = 256) in;

layout (set = 0, binding = 0) uniform usampler2DArray u_hzb;

layout (scalar, buffer_reference) restrict buffer b_vsm_dispatch_command_block {
    draw_mesh_tasks_indirect_command_t data;
};

layout (scalar, push_constant) restrict uniform u_push_constant_block {
    restrict readonly b_view_block u_view_ptr;
    restrict readonly b_meshlet_instance_block u_meshlet_instance_ptr;
    restrict readonly b_meshlet_block u_meshlet_ptr;
    restrict readonly b_transform_block u_transform_ptr;
    restrict writeonly b_vsm_meshlet_survivors_block b_vsm_meshlet_survivors;
    restrict b_vsm_dispatch_command_block b_vsm_dispatch_command;
    uint u_meshlet_count;
    uint u_clipmap_index;
};

vec4 project_screen_aabb(in aabb_t aabb, in mat4 transform, in mat4 proj_view) {
    const vec3[] corners = vec3[](
        vec3(aabb.min.x, aabb.min.y, aabb.min.z),
        vec3(aabb.min.x, aabb.min.y, aabb.max.z),
        vec3(aabb.min.x, aabb.max.y, aabb.min.z),
        vec3(aabb.min.x, aabb.max.y, aabb.max.z),
        vec3(aabb.max.x, aabb.min.y, aabb.min.z),
        vec3(aabb.max.x, aabb.min.y, aabb.max.z),
        vec3(aabb.max.x, aabb.max.y, aabb.min.z),
        vec3(aabb.max.x, aabb.max.y, aabb.max.z)
    );
    vec2 min_xy = vec2(1.0);
    vec2 max_xy = vec2(0.0);
    for (uint i = 0; i < 8; ++i) {
        const vec4 clip = proj_view * transform * vec4(corners[i], 1.0);
        const vec2 uv = fract((clip.xy / clip.w) * 0.5 + 0.5);
        min_xy = min(min_xy, uv);
        max_xy = max(max_xy, uv);
    }
    return vec4(min_xy, max_xy);
}

bool is_meshlet_visible(in vec4 box_uvs) {
    const vec2 min_xy = box_uvs.xy;
    const vec2 max_xy = box_uvs.zw;
    const vec2 hzb_size = vec2(textureSize(u_hzb, 0));
    const float width = (max_xy.x - min_xy.x) * hzb_size.x;
    const float height = (max_xy.y - min_xy.y) * hzb_size.y;
    const float level = ceil(log2(max(width, height)));
    const bvec4 samples = bvec4(
        bool(textureLod(u_hzb, vec3(box_uvs.xy, u_clipmap_index), level).x),
        bool(textureLod(u_hzb, vec3(box_uvs.zy, u_clipmap_index), level).x),
        bool(textureLod(u_hzb, vec3(box_uvs.xw, u_clipmap_index), level).x),
        bool(textureLod(u_hzb, vec3(box_uvs.zw, u_clipmap_index), level).x)
    );
    return any(samples);
}

void main() {
    if (uint(textureLod(u_hzb, vec3(vec2(0.5), u_clipmap_index), IRIS_LAST_MIP_LEVEL)) == 0) {
        return;
    }
    if (gl_GlobalInvocationID.x == 0) {
        b_vsm_dispatch_command.data.y = 1;
        b_vsm_dispatch_command.data.z = 1;
    }

    const uint meshlet_instance_id = gl_GlobalInvocationID.x;
    if (meshlet_instance_id >= u_meshlet_count) {
        return;
    }

    const uint meshlet_id = u_meshlet_instance_ptr.data[meshlet_instance_id].meshlet_id;
    const uint instance_id = u_meshlet_instance_ptr.data[meshlet_instance_id].instance_id;

    const aabb_t aabb = u_meshlet_ptr.data[meshlet_id].aabb;
    const mat4 transform = u_transform_ptr.data[instance_id].model;
    const mat4 proj_view = u_view_ptr.data[IRIS_SHADOW_VIEW_START_INDEX + u_clipmap_index].stable_proj_view;

    if (is_meshlet_visible(project_screen_aabb(aabb, transform, proj_view))) {
        const uint slot = atomicAdd(b_vsm_dispatch_command.data.x, 1);
        b_vsm_meshlet_survivors.data[slot] = meshlet_instance_id;
    }
}
