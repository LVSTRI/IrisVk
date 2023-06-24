#version 460
#extension GL_ARB_separate_shader_objects : enable

struct camera_data_t {
    mat4 projection;
    mat4 view;
    mat4 pv;
    vec4 position;
};

layout (location = 0) out vec3 o_color;

const vec3[] positions = vec3[](
    vec3(-0.5, -0.5, 1.0),
    vec3( 0.5, -0.5, 1.0),
    vec3( 0.0,  0.5, 1.0)
);

const vec3[] colors = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout (set = 0, binding = 0) uniform u_camera_block {
    camera_data_t data;
} u_camera;

void main() {
    gl_Position = u_camera.data.pv * vec4(positions[gl_VertexIndex], 1.0);
    o_color = colors[gl_VertexIndex];
}
