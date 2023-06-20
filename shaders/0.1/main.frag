#version 460
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 i_color;

layout (location = 0) out vec4 o_pixel;

void main() {
    o_pixel = vec4(i_color, 1.0);
}
