#version 460

layout (location = 0) in vec3 i_position;
layout (location = 1) in vec3 i_color;

layout (location = 0) out vec3 o_color;

void main() {
    gl_Position = vec4(i_position, 1.0);
    o_color = i_color;
}