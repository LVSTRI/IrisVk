#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_mesh_shader : enable
#extension GL_KHR_shader_subgroup : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "visbuffer/common.glsl"
#include "data.glsl"
#include "buffer.glsl"

#define WORK_GROUP_SIZE 32
#define MAX_INDICES_PER_THREAD ((CLUSTER_MAX_VERTICES + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE)
#define MAX_PRIMITIVES_PER_THREAD ((CLUSTER_MAX_PRIMITIVES + WORK_GROUP_SIZE - 1) / WORK_GROUP_SIZE)

layout (local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout (triangles, max_vertices = CLUSTER_MAX_VERTICES, max_primitives = CLUSTER_MAX_PRIMITIVES) out;

layout (location = 0) out o_vertex_data_block {
    flat uint meshlet_id;
} o_vertex_data[];

layout (scalar, buffer_reference) restrict readonly buffer IRIS_GLSL_VIEW_BUFFER_BLOCK(b_view_block);
layout (scalar, buffer_reference) restrict readonly buffer IRIS_GLSL_MESHLET_INSTANCE_BUFFER_BLOCK(b_meshlet_instance_block);
layout (scalar, buffer_reference) restrict readonly buffer IRIS_GLSL_MESHLET_BUFFER_BLOCK(b_meshlet_block);
layout (scalar, buffer_reference) restrict readonly buffer IRIS_GLSL_TRANSFORM_BUFFER_BLOCK(b_transform_block);
layout (scalar, buffer_reference) restrict readonly buffer IRIS_GLSL_VERTEX_BUFFER_BLOCK(b_vertex_block);
layout (scalar, buffer_reference) restrict readonly buffer IRIS_GLSL_INDEX_BUFFER_BLOCK(b_index_block);
layout (scalar, buffer_reference) restrict readonly buffer IRIS_GLSL_PRIMITIVE_BUFFER_BLOCK(b_primitive_block);
layout (scalar, buffer_reference) restrict readonly buffer IRIS_GLSL_ADDRESS_BUFFER_BLOCK(b_address_block);

layout (push_constant) uniform u_push_constants_block {
     b_address_block u_address_ptr;
};

shared uint vertex_offset;
shared uint index_offset;
shared uint primitive_offset;
shared uint index_count;
shared uint primitive_count;
shared mat4 pvm;

void main() {
    const uint thread_index = gl_LocalInvocationID.x;
    const uint meshlet_instance_id = gl_WorkGroupID.x;

    restrict b_view_block view_ptr = b_view_block(u_address_ptr.data[IRIS_GLSL_VIEW_BUFFER_SLOT]);
    restrict b_meshlet_instance_block meshlet_instance_ptr = b_meshlet_instance_block(u_address_ptr.data[IRIS_GLSL_MESHLET_INSTANCE_BUFFER_SLOT]);
    restrict b_meshlet_block meshlet_ptr = b_meshlet_block(u_address_ptr.data[IRIS_GLSL_MESHLET_BUFFER_SLOT]);
    restrict b_transform_block transform_ptr = b_transform_block(u_address_ptr.data[IRIS_GLSL_TRANSFORM_BUFFER_SLOT]);
    restrict b_vertex_block vertex_ptr = b_vertex_block(u_address_ptr.data[IRIS_GLSL_VERTEX_BUFFER_SLOT]);
    restrict b_index_block index_ptr = b_index_block(u_address_ptr.data[IRIS_GLSL_INDEX_BUFFER_SLOT]);
    restrict b_primitive_block primitive_ptr = b_primitive_block(u_address_ptr.data[IRIS_GLSL_PRIMITIVE_BUFFER_SLOT]);

    if (thread_index == 0) {
        const uint meshlet_id = meshlet_instance_ptr.data[meshlet_instance_id].meshlet_id;
        const uint instance_id = meshlet_instance_ptr.data[meshlet_instance_id].instance_id;
        const view_t main_view = view_ptr.data[IRIS_GLSL_MAIN_VIEW_INDEX];

        const meshlet_t meshlet = meshlet_ptr.data[meshlet_id];
        const mat4 transform = transform_ptr.data[instance_id].model;
        vertex_offset = meshlet.vertex_offset;
        index_offset = meshlet.index_offset;
        primitive_offset = meshlet.primitive_offset;
        index_count = meshlet.index_count;
        primitive_count = meshlet.primitive_count;
        pvm = main_view.proj_view * transform;
        SetMeshOutputsEXT(index_count, primitive_count);
    }
    barrier();

    for (uint i = 0; i < MAX_INDICES_PER_THREAD; i++) {
        // avoid branching, get pipelined memory loads
        const uint index = min(thread_index + i * WORK_GROUP_SIZE, index_count - 1);
        const vec3 position = vertex_ptr.data[vertex_offset + index_ptr.data[index_offset + index]].position;

        gl_MeshVerticesEXT[index].gl_Position = pvm * vec4(position, 1.0);
        o_vertex_data[index].meshlet_id = meshlet_instance_id;
    }

    for (uint i = 0; i < MAX_PRIMITIVES_PER_THREAD; i++) {
        const uint index = min(thread_index + i * WORK_GROUP_SIZE, primitive_count - 1);
        gl_PrimitiveTriangleIndicesEXT[index] = uvec3(
            uint(primitive_ptr.data[primitive_offset + index * 3 + 0]),
            uint(primitive_ptr.data[primitive_offset + index * 3 + 1]),
            uint(primitive_ptr.data[primitive_offset + index * 3 + 2]));
        gl_MeshPrimitivesEXT[index].gl_PrimitiveID = int(index);
    }
}
