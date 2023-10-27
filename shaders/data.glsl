#ifndef IRIS_GLSL_DATA_HEADER
#define IRIS_GLSL_DATA_HEADER

struct draw_mesh_tasks_indirect_command_t {
    uint x;
    uint y;
    uint z;
};

struct compute_indirect_command_t {
    uint x;
    uint y;
    uint z;
};

struct view_t {
    mat4 projection;
    mat4 inv_projection;
    mat4 view;
    mat4 inv_view;
    mat4 proj_view;
    mat4 inv_proj_view;
    vec4 eye_position;
    vec4[6] frustum;
    vec2 resolution;
};

struct aabb_t {
    vec3 min;
    vec3 max;
};

struct vertex_format_t {
    vec3 position;
    vec3 normal;
    vec2 uv;
};

struct meshlet_t {
    uint vertex_offset;
    uint index_offset;
    uint primitive_offset;
    uint index_count;
    uint primitive_count;
    aabb_t aabb;
};

struct meshlet_instance_t {
    uint meshlet_id;
    uint instance_id;
    uint material_id;
};

struct transform_t {
    mat4 model;
    mat4 prev_model;
};

struct material_t {
    uint base_color_texture;
    uint normal_texture;
    vec3 base_color_factor;
};

struct directional_light_t {
    vec3 direction;
    float intensity;
};

#endif
