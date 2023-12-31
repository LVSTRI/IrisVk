#define M_GOLDEN_CONJ 0.6180339887498948482045868343656
#define MAX_VERTICES 64
#define MAX_PRIMITIVES 64
#define CLUSTER_CLASSIFY_ATOMIC_SLOT 0
#define VMM_ARGS(scope, storage, semantics) gl_Scope##scope, gl_StorageSemantics##storage, gl_Semantics##semantics

#define VSM_TILE_SIZE_X 128
#define VSM_TILE_SIZE_Y 128
#define VSM_RESOLUTION 16384
#define VSM_PAGE_COUNT (16384 / VSM_TILE_SIZE_X)

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

struct camera_data_t {
    mat4 projection;
    mat4 view;
    mat4 old_pv;
    mat4 pv;
    vec4 position;
    vec4[6] frustum;
};

struct aabb_t {
    float[3] min;
    float[3] max;
};

struct meshlet_material_glsl_t {
    uint base_color_texture;
    uint normal_texture;
};

struct meshlet_glsl_t {
    uint vertex_offset;
    uint index_offset;
    uint primitive_offset;
    uint index_count;
    uint primitive_count;
    aabb_t aabb;
    meshlet_material_glsl_t material;
};

struct meshlet_instance_t {
    uint meshlet_id;
    uint instance_id;
};

struct vertex_format_t {
    float[3] position;
    float[3] normal;
    float[2] uv;
};

float saturate(in float v) {
    return clamp(v, 0.0, 1.0);
}

vec2 saturate(in vec2 v) {
    return clamp(v, 0.0, 1.0);
}

vec3 saturate(in vec3 v) {
    return clamp(v, 0.0, 1.0);
}

vec4 saturate(in vec4 v) {
    return clamp(v, 0.0, 1.0);
}

vec3 hsv_to_rgb(in vec3 hsv) {
    const vec3 rgb = saturate(abs(mod(hsv.x * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0);
    return hsv.z * mix(vec3(1.0), rgb, hsv.y);
}

vec3 unproject_depth(in mat4 pv, in float depth, in vec2 uv) {
    const vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    const vec4 world = inverse(pv) * ndc;
    return world.xyz / world.w;
}

vec2 vec2_from_float(in float[2] v) {
    return vec2(v[0], v[1]);
}

vec3 vec3_from_float(in float[3] v) {
    return vec3(v[0], v[1], v[2]);
}

vec4 vec4_from_float(in float[4] v) {
    return vec4(v[0], v[1], v[2], v[3]);
}
