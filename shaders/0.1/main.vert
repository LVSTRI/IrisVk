#version 460

const vec3[] vertices = vec3[](
    vec3(-0.5,  0.5, 0.0),
    vec3( 0.5,  0.5, 0.0),
    vec3( 0.0, -0.5, 0.0));

const vec3[] colors = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0));

layout (location = 0) out vec3 o_color;

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 1.0);
    o_color = colors[gl_VertexIndex];
}