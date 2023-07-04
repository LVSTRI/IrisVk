#version 460
#extension GL_EXT_mesh_shader : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
//#extension GL_EXT_debug_printf : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#define TASK_WORKGROUP_SIZE 32

#include "common.glsl"

layout (local_size_x = TASK_WORKGROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout (set = 0, binding = 0) uniform u_camera_block {
    camera_data_t data;
} u_camera;

layout (set = 0, binding = 1) uniform sampler2D u_hiz;

layout (scalar, buffer_reference) restrict readonly buffer b_meshlet_buffer {
    meshlet_glsl_t[] data;
};

layout (scalar, buffer_reference) restrict readonly buffer b_meshlet_instance_buffer {
    meshlet_instance_t[] data;
};

layout (scalar, buffer_reference) restrict readonly buffer b_transform_buffer {
    mat4[] data;
};

layout (push_constant) uniform pc_data_block {
    uint64_t meshlet_address;
    uint64_t meshlet_instance_address;
    uint64_t vertex_address;
    uint64_t index_address;
    uint64_t primitive_address;
    uint64_t transforms_address;
    uint meshlet_count;
};

struct task_payload_t {
    uint base_id;
    uint8_t offset[TASK_WORKGROUP_SIZE];
};

taskPayloadSharedEXT task_payload_t payload;

bool is_aabb_inside_plane(in vec3 center, in vec3 extent, in vec4 plane) {
    const vec3 normal = plane.xyz;
    const float radius = dot(extent, abs(normal));
    return (dot(normal, center) - plane.w) >= -radius;
}

bool is_meshlet_occluded(in aabb_t aabb, in mat4 model) {
    const vec3 aabb_min = vec3_from_float(aabb.min);
    const vec3 aabb_max = vec3_from_float(aabb.max);
    const vec3 aabb_size = aabb_max - aabb_min;
    const vec3[] corners = vec3[](
        aabb_min,
        aabb_min + vec3(aabb_size.x, 0.0, 0.0),
        aabb_min + vec3(0.0, aabb_size.y, 0.0),
        aabb_min + vec3(0.0, 0.0, aabb_size.z),
        aabb_min + vec3(aabb_size.xy, 0.0),
        aabb_min + vec3(0.0, aabb_size.yz),
        aabb_min + vec3(aabb_size.x, 0.0, aabb_size.z),
        aabb_max);
    float max_z = 0.0;
    vec2 min_xy = vec2(1.0);
    vec2 max_xy = vec2(0.0);
    for (uint i = 0; i < 8; ++i) {
        vec4 clip_pos = u_camera.data.old_pv * model * vec4(corners[i], 1.0);
        clip_pos.z = max(clip_pos.z, 0.0);
        clip_pos /= clip_pos.w;
        clip_pos.xy = clamp(clip_pos.xy, -1.0, 1.0);
        clip_pos.xy = clip_pos.xy * vec2(0.5, -0.5) + 0.5;
        min_xy = min(min_xy, clip_pos.xy);
        max_xy = max(max_xy, clip_pos.xy);
        max_z = clamp(max(max_z, clip_pos.z), 0.0, 1.0);
    }
    const vec4 box_uvs = vec4(min_xy, max_xy);
    const vec2 hiz_size = textureSize(u_hiz, 0);
    const float width = (box_uvs.z - box_uvs.x) * hiz_size.x;
    const float height = (box_uvs.w - box_uvs.y) * hiz_size.y;
    const float level = floor(log2(max(width, height)));
    // min reduction
    const float depth = textureLod(u_hiz, (box_uvs.xy + box_uvs.zw) / 2.0, level).r;
    return max_z > depth;
}

bool is_meshlet_visible(in uint meshlet_id, in uint instance_id) {
    restrict b_meshlet_buffer meshlet_ptr = b_meshlet_buffer(meshlet_address);
    restrict b_transform_buffer transform_ptr = b_transform_buffer(transforms_address);
    const aabb_t aabb = meshlet_ptr.data[meshlet_id].aabb;
    const mat4 model = transform_ptr.data[instance_id];

    const vec3 aabb_min = vec3_from_float(aabb.min);
    const vec3 aabb_max = vec3_from_float(aabb.max);
    const vec3 aabb_center = (aabb_max + aabb_min) / 2.0;
    const vec3 aabb_extent = aabb_max - aabb_center;
    const vec3 world_aabb_center = vec3(model * vec4(aabb_center, 1.0));
    const vec3 right = vec3(model[0]) * aabb_extent.x;
    const vec3 up = vec3(model[1]) * aabb_extent.y;
    const vec3 forward = vec3(-model[2]) * aabb_extent.z;

    const vec3 world_extent = vec3(
        abs(dot(vec3(1.0, 0.0, 0.0), right)) +
        abs(dot(vec3(1.0, 0.0, 0.0), up)) +
        abs(dot(vec3(1.0, 0.0, 0.0), forward)),

        abs(dot(vec3(0.0, 1.0, 0.0), right)) +
        abs(dot(vec3(0.0, 1.0, 0.0), up)) +
        abs(dot(vec3(0.0, 1.0, 0.0), forward)),

        abs(dot(vec3(0.0, 0.0, 1.0), right)) +
        abs(dot(vec3(0.0, 0.0, 1.0), up)) +
        abs(dot(vec3(0.0, 0.0, 1.0), forward)));

    // TODO: near plane should be optional (shadows)
    const uint planes = 6;
    for (uint i = 0; i < planes; ++i) {
        if (!is_aabb_inside_plane(world_aabb_center, world_extent, u_camera.data.frustum[i])) {
            return false;
        }
    }
    return is_meshlet_occluded(aabb, model);
}

void main() {
    const uint meshlet_instance_id = gl_GlobalInvocationID.x;
    restrict b_meshlet_instance_buffer instance_ptr = b_meshlet_instance_buffer(meshlet_instance_address);
    const uint meshlet_id = instance_ptr.data[meshlet_instance_id].meshlet_id;
    const uint instance_id = instance_ptr.data[meshlet_instance_id].instance_id;
    const bool is_visible = meshlet_instance_id < meshlet_count && is_meshlet_visible(meshlet_id, instance_id);
    const uvec4 vote = subgroupBallot(is_visible);
    const uint surviving = subgroupBallotBitCount(vote);
    const uint offset_index = subgroupBallotExclusiveBitCount(vote);
    if (is_visible) {
        payload.offset[offset_index] = uint8_t(gl_LocalInvocationID.x);
    }
    if (gl_LocalInvocationID.x == 0) {
        payload.base_id = gl_WorkGroupID.x * TASK_WORKGROUP_SIZE;
        EmitMeshTasksEXT(surviving, 1, 1);
    }
}
