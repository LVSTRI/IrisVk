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
