#version 460
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec3 o_color;

const vec3[] positions = vec3[](
    vec3(-0.5,  0.5, 1.0),
    vec3( 0.5,  0.5, 1.0),
    vec3( 0.0, -0.5, 1.0)
);

const vec3[] colors = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 1.0);
    o_color = colors[gl_VertexIndex];
}
