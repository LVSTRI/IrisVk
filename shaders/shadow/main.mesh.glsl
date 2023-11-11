#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_mesh_shader : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "visbuffer/common.glsl"
#include "buffer.glsl"

#define WORK_GROUP_SIZE 32
#define MAX_INDICES_PER_THREAD ((CLUSTER_MAX_VERTICES + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE)
#define MAX_PRIMITIVES_PER_THREAD ((CLUSTER_MAX_PRIMITIVES + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE)

layout (local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout (triangles, max_vertices = CLUSTER_MAX_VERTICES, max_primitives = CLUSTER_MAX_PRIMITIVES) out;

layout (scalar, push_constant) restrict readonly uniform u_push_constants_block {
    restrict readonly b_shadow_cascade_block u_shadow_cascade_ptr;
    restrict readonly b_meshlet_instance_block u_meshlet_instance_ptr;
    restrict readonly b_meshlet_block u_meshlet_ptr;
    restrict readonly b_transform_block u_transform_ptr;
    restrict readonly b_position_block u_position_ptr;
    restrict readonly b_index_block u_index_ptr;
    restrict readonly b_primitive_block u_primitive_ptr;
    uint u_layer;
};

void main() {
    const uint meshlet_instance_id = gl_WorkGroupID.x;

    const meshlet_instance_t meshlet_info = u_meshlet_instance_ptr.data[meshlet_instance_id];
    const shadow_cascade_t view = u_shadow_cascade_ptr.data[u_layer];
    const meshlet_t meshlet = u_meshlet_ptr.data[meshlet_info.meshlet_id];
    const transform_t transform = u_transform_ptr.data[meshlet_info.instance_id];
    const mat4 pvm = view.proj_view * transform.model;

    if (gl_LocalInvocationID.x == 0) {
        SetMeshOutputsEXT(meshlet.index_count, meshlet.primitive_count);
    }
    barrier();

    for (uint i = 0; i < MAX_INDICES_PER_THREAD; i++) {
        // avoid branching, get pipelined memory loads
        const uint index = min(gl_LocalInvocationID.x + i * WORK_GROUP_SIZE, meshlet.index_count - 1);
        const vec3 position = u_position_ptr.data[meshlet.vertex_offset + u_index_ptr.data[meshlet.index_offset + index]];
        gl_MeshVerticesEXT[index].gl_Position = pvm * vec4(position, 1.0);
    }

    for (uint i = 0; i < MAX_PRIMITIVES_PER_THREAD; i++) {
        const uint index = min(gl_LocalInvocationID.x + i * WORK_GROUP_SIZE, meshlet.primitive_count - 1);
        gl_PrimitiveTriangleIndicesEXT[index] = uvec3(
            uint(u_primitive_ptr.data[meshlet.primitive_offset + index * 3 + 0]),
            uint(u_primitive_ptr.data[meshlet.primitive_offset + index * 3 + 1]),
            uint(u_primitive_ptr.data[meshlet.primitive_offset + index * 3 + 2]));
        gl_MeshPrimitivesEXT[index].gl_PrimitiveID = int(index);
        gl_MeshPrimitivesEXT[index].gl_Layer = int(u_layer);
    }
}
