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

layout (scalar, push_constant) restrict uniform u_push_constants_block {
    restrict readonly b_view_block u_view_ptr;
    restrict readonly b_meshlet_instance_block u_meshlet_instance_ptr;
    restrict readonly b_meshlet_block u_meshlet_ptr;
    restrict readonly b_transform_block u_transform_ptr;
    restrict readonly b_vertex_block u_vertex_ptr;
    restrict readonly b_index_block u_index_ptr;
    restrict readonly b_primitive_block u_primitive_ptr;
    restrict readonly b_vsm_virtual_page_table_block u_virt_page_table_ptr;
    restrict readonly b_vsm_meshlet_survivors_block u_meshlet_survivors_ptr;
    restrict readonly b_vsm_globals_block u_vsm_globals_ptr;

    uint u_view_index;
};

shared uint vertex_offset;
shared uint index_offset;
shared uint primitive_offset;
shared uint index_count;
shared uint primitive_count;
shared mat4 pvm;

void main() {
    const uint thread_index = gl_LocalInvocationID.x;
    const uint meshlet_instance_id = u_meshlet_survivors_ptr.data[gl_WorkGroupID.x];

    if (thread_index == 0) {
        const uint meshlet_id = u_meshlet_instance_ptr.data[meshlet_instance_id].meshlet_id;
        const uint instance_id = u_meshlet_instance_ptr.data[meshlet_instance_id].instance_id;
        const view_t view = u_view_ptr.data[IRIS_SHADOW_VIEW_START_INDEX + u_view_index];

        const meshlet_t meshlet = u_meshlet_ptr.data[meshlet_id];
        const mat4 transform = u_transform_ptr.data[instance_id].model;
        vertex_offset = meshlet.vertex_offset;
        index_offset = meshlet.index_offset;
        primitive_offset = meshlet.primitive_offset;
        index_count = meshlet.index_count;
        primitive_count = meshlet.primitive_count;
        pvm = view.proj_view * transform;
        SetMeshOutputsEXT(index_count, primitive_count);
    }
    barrier();

    for (uint i = 0; i < MAX_INDICES_PER_THREAD; i++) {
        // avoid branching, get pipelined memory loads
        const uint index = min(thread_index + i * WORK_GROUP_SIZE, index_count - 1);
        const vec3 position = u_vertex_ptr.data[vertex_offset + u_index_ptr.data[index_offset + index]].position;

        gl_MeshVerticesEXT[index].gl_Position = pvm * vec4(position, 1.0);
    }

    for (uint i = 0; i < MAX_PRIMITIVES_PER_THREAD; i++) {
        const uint index = min(thread_index + i * WORK_GROUP_SIZE, primitive_count - 1);
        gl_PrimitiveTriangleIndicesEXT[index] = uvec3(
            uint(u_primitive_ptr.data[primitive_offset + index * 3 + 0]),
            uint(u_primitive_ptr.data[primitive_offset + index * 3 + 1]),
            uint(u_primitive_ptr.data[primitive_offset + index * 3 + 2]));
        gl_MeshPrimitivesEXT[index].gl_PrimitiveID = int(index);
    }
}
