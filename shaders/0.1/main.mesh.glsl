#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_mesh_shader : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "common.glsl"

#define WORKGROUP_SIZE 32
#define MAX_VERTICES 40
#define MAX_PRIMITIVES 84
#define MAX_INDICES_PER_THREAD ((MAX_VERTICES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE)
#define MAX_PRIMITIVES_PER_THREAD ((MAX_PRIMITIVES + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE)

layout (local_size_x = WORKGROUP_SIZE, local_size_y = 1, local_size_z = 1) in;

layout (triangles, max_vertices = MAX_VERTICES, max_primitives = MAX_PRIMITIVES) out;

layout (location = 0) out o_vertex_data_block {
    flat uint meshlet_id;
} o_vertex_data[];

layout (set = 0, binding = 0) uniform u_camera_block {
    camera_data_t data;
} u_camera;

layout (scalar, buffer_reference) restrict readonly buffer b_meshlet_buffer {
    meshlet_glsl_t[] data;
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

layout (push_constant) uniform pc_address_block {
    uint64_t meshlet_address;
    uint64_t vertex_address;
    uint64_t index_address;
    uint64_t primitive_address;
    uint64_t transforms_address;
    uint meshlet_count;
    uint _pad;
};

taskPayloadSharedEXT task_payload_t payload;

void main() {
    const uint meshlet_id = payload.base_meshlet_id + payload.meshlet_offset[gl_WorkGroupID.x];
    const uint thread_index = gl_LocalInvocationID.x;

    restrict b_meshlet_buffer meshlet_ptr = b_meshlet_buffer(meshlet_address);
    restrict b_vertex_buffer vertex_ptr = b_vertex_buffer(vertex_address);
    restrict b_index_buffer index_ptr = b_index_buffer(index_address);
    restrict b_primitive_buffer primitive_ptr = b_primitive_buffer(primitive_address);
    restrict b_transform_buffer transform_ptr = b_transform_buffer(transforms_address);

    const meshlet_glsl_t meshlet = meshlet_ptr.data[meshlet_id];
    const uint vertex_offset = meshlet.vertex_offset;
    const uint index_offset = meshlet.index_offset;
    const uint primitive_offset = meshlet.primitive_offset;
    const uint index_count = meshlet.index_count;
    const uint primitive_count = meshlet.primitive_count;
    const uint group_id = meshlet.group_id;

    if (thread_index == 0) {
        SetMeshOutputsEXT(index_count, primitive_count);
    }

    for (uint i = 0; i < MAX_INDICES_PER_THREAD; i++) {
        const uint index = min(thread_index + i * WORKGROUP_SIZE, index_count - 1);
        const vertex_format_t vertex = vertex_ptr.data[vertex_offset + index_ptr.data[index_offset + index]];
        const vec3 position = vec3_from_float(vertex.position);
        const mat4 transform = transform_ptr.data[group_id];

        gl_MeshVerticesEXT[index].gl_Position = u_camera.data.pv * transform * vec4(position, 1.0);
        o_vertex_data[index].meshlet_id = meshlet_id;
    }

    // gl_PrimitiveTriangleIndicesEXT
    for (uint i = 0; i < MAX_PRIMITIVES_PER_THREAD; i++) {
        const uint index = min(thread_index + i * WORKGROUP_SIZE, primitive_count - 1);
        gl_PrimitiveTriangleIndicesEXT[index] = uvec3(
            uint(primitive_ptr.data[primitive_offset + index * 3 + 0]),
            uint(primitive_ptr.data[primitive_offset + index * 3 + 1]),
            uint(primitive_ptr.data[primitive_offset + index * 3 + 2]));
        gl_MeshPrimitivesEXT[index].gl_PrimitiveID = int(index);
    }
}
