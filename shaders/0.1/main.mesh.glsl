#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_mesh_shader : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "common.glsl"

#define WORKGROUP_SIZE 32
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

layout (scalar, buffer_reference) restrict readonly buffer b_hw_meshlet_offset {
    uint count;
    uint[] data;
};

layout (push_constant) uniform pc_data_block {
    restrict b_meshlet_buffer meshlet_ptr;
    restrict b_meshlet_instance_buffer instance_ptr;
    restrict b_vertex_buffer vertex_ptr;
    restrict b_index_buffer index_ptr;
    restrict b_primitive_buffer primitive_ptr;
    restrict b_transform_buffer transform_ptr;
    restrict b_hw_meshlet_offset hw_meshlet_offset_ptr;
};

shared uint vertex_offset;
shared uint index_offset;
shared uint primitive_offset;
shared uint index_count;
shared uint primitive_count;
shared mat4 pvm;

void main() {
    const uint meshlet_instance_id = hw_meshlet_offset_ptr.data[gl_WorkGroupID.x];
    const uint thread_index = gl_LocalInvocationID.x;

    if (thread_index == 0) {
        const uint meshlet_id = instance_ptr.data[meshlet_instance_id].meshlet_id;
        const uint instance_id = instance_ptr.data[meshlet_instance_id].instance_id;

        const meshlet_glsl_t meshlet = meshlet_ptr.data[meshlet_id];
        const mat4 transform = transform_ptr.data[instance_id];
        vertex_offset = meshlet.vertex_offset;
        index_offset = meshlet.index_offset;
        primitive_offset = meshlet.primitive_offset;
        index_count = meshlet.index_count;
        primitive_count = meshlet.primitive_count;
        pvm = u_camera.data.pv * transform;
        SetMeshOutputsEXT(index_count, primitive_count);
    }
    barrier();

    for (uint i = 0; i < MAX_INDICES_PER_THREAD; i++) {
        // avoid branching, get pipelined memory loads
        const uint index = min(thread_index + i * WORKGROUP_SIZE, index_count - 1);
        const vec3 position = vec3_from_float(vertex_ptr.data[vertex_offset + index_ptr.data[index_offset + index]].position);

        gl_MeshVerticesEXT[index].gl_Position = pvm * vec4(position, 1.0);
        o_vertex_data[index].meshlet_id = meshlet_instance_id;
    }

    // gl_PrimitiveTriangleIndicesEXT
    for (uint i = 0; i < MAX_PRIMITIVES_PER_THREAD; i++) {
        // avoid branching, get pipelined memory loads
        const uint index = min(thread_index + i * WORKGROUP_SIZE, primitive_count - 1);
        gl_PrimitiveTriangleIndicesEXT[index] = uvec3(
            uint(primitive_ptr.data[primitive_offset + index * 3 + 0]),
            uint(primitive_ptr.data[primitive_offset + index * 3 + 1]),
            uint(primitive_ptr.data[primitive_offset + index * 3 + 2]));
        gl_MeshPrimitivesEXT[index].gl_PrimitiveID = int(index);
    }
}
