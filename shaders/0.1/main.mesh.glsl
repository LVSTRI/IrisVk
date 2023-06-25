#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_mesh_shader : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

struct camera_data_t {
    mat4 projection;
    mat4 view;
    mat4 pv;
    vec4 position;
};

struct meshlet_glsl_t {
    uint vertex_offset;
    uint index_offset;
    uint primitive_offset;
    uint index_count;
    uint primitive_count;
    uint group_id;
};

struct vertex_format_t {
    float[3] position;
    float[3] normal;
    float[2] uv;
    float[4] tangent;
};

#define LOCAL_THREAD_COUNT 64
#define MAX_VERTICES 64
#define MAX_PRIMITIVES 124

layout (local_size_x = LOCAL_THREAD_COUNT, local_size_y = 1, local_size_z = 1) in;

layout (triangles, max_vertices = MAX_VERTICES, max_primitives = MAX_PRIMITIVES) out;

layout (location = 0) out o_vertex_data_block {
    flat uint meshlet_id;
    flat uint group_id;
    vec3 normal;
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
};

const vec3[] colors = vec3[](vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0));

void main() {
    const uint meshlet_index = gl_WorkGroupID.x;
    const uint thread_index = gl_LocalInvocationID.x;

    restrict b_meshlet_buffer meshlet_ptr = b_meshlet_buffer(meshlet_address);
    restrict b_vertex_buffer vertex_ptr = b_vertex_buffer(vertex_address);
    restrict b_index_buffer index_ptr = b_index_buffer(index_address);
    restrict b_primitive_buffer primitive_ptr = b_primitive_buffer(primitive_address);
    restrict b_transform_buffer transform_ptr = b_transform_buffer(transforms_address);

    const meshlet_glsl_t meshlet = meshlet_ptr.data[meshlet_index];
    const uint vertex_offset = meshlet.vertex_offset;
    const uint index_offset = meshlet.index_offset;
    const uint primitive_offset = meshlet.primitive_offset;
    const uint index_count = meshlet.index_count;
    const uint primitive_count = meshlet.primitive_count;
    const uint group_id = meshlet.group_id;

    if (thread_index < index_count) {
        const vertex_format_t vertex = vertex_ptr.data[vertex_offset + index_ptr.data[index_offset + thread_index]];
        const vec3 position = vec3(
            vertex.position[0],
            vertex.position[1],
            vertex.position[2]);
        const mat4 transform = transform_ptr.data[group_id];

        gl_MeshVerticesEXT[thread_index].gl_Position = u_camera.data.pv * transform * vec4(position, 1.0);
        o_vertex_data[thread_index].meshlet_id = meshlet_index;
        o_vertex_data[thread_index].group_id = group_id;
        o_vertex_data[thread_index].normal = mat3(transform) * vec3(
            vertex.normal[0],
            vertex.normal[1],
            vertex.normal[2]);
    }

    // gl_PrimitiveTriangleIndicesEXT
    const uint max_primitives_per_thread = primitive_count / LOCAL_THREAD_COUNT + 1;
    if (thread_index * max_primitives_per_thread < primitive_count) {
        for (uint i = 0; i < max_primitives_per_thread; i++) {
            const uint index = thread_index * max_primitives_per_thread + i;
            gl_PrimitiveTriangleIndicesEXT[index] = uvec3(
                uint(primitive_ptr.data[primitive_offset + index * 3 + 0]),
                uint(primitive_ptr.data[primitive_offset + index * 3 + 1]),
                uint(primitive_ptr.data[primitive_offset + index * 3 + 2]));
            gl_MeshPrimitivesEXT[index].gl_PrimitiveID = int(index);
        }
    }

    if (thread_index == 0) {
        SetMeshOutputsEXT(index_count, primitive_count);
    }
}
