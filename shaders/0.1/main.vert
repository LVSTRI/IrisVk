#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

struct camera_data_t {
    mat4 projection;
    mat4 view;
    mat4 pv;
    vec4 position;
};

struct vertex_format_t {
    float[3] position;
    float[3] color;
};

layout (location = 0) out vec3 o_color;

layout (set = 0, binding = 0) uniform u_camera_block {
    camera_data_t data;
} u_camera;

layout (buffer_reference) restrict readonly buffer b_vertex_buffer {
    vertex_format_t[] data;
};

layout (buffer_reference) restrict readonly buffer b_index_buffer {
    uint[] data;
};

layout (push_constant) uniform pc_address_block {
    uint64_t vertex_address;
    uint64_t index_address;
};

void main() {
    restrict b_vertex_buffer vertex_ptr = b_vertex_buffer(vertex_address);
    restrict b_index_buffer index_ptr = b_index_buffer(index_address);
    const restrict vertex_format_t vertex = vertex_ptr.data[gl_VertexIndex];
    const vec3 position = vec3(
        vertex.position[0],
        vertex.position[1],
        vertex.position[2]
    );
    const vec3 color = vec3(
        vertex.color[0],
        vertex.color[1],
        vertex.color[2]
    );
    gl_Position = u_camera.data.pv * vec4(position, 1.0);
    o_color = color;
}
