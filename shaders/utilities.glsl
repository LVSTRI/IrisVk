#ifndef IRIS_UTILITIES_HEADER
#define IRIS_UTILITIES_HEADER

#define M_PI 3.1415926535897932384626433832795
#define M_TWOPI 6.283185307179586476925286766559
#define M_GOLDEN 1.6180339887498948482045868343656
#define M_GOLDEN_CONJ 0.6180339887498948482045868343656
#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38
#define FLT_LOWEST -3.402823466e+38

#define IRIS_SIMPLE_HASH(p) fract(sin(dot(p, vec2(11.9898, 78.233))) * 43758.5453)

#define IRIS_LAST_MIP_LEVEL 1024.0

uint __iris_pcg_state = 0;

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

float luminance(in vec3 rgb) {
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

vec3 linear_as_srgb(in vec3 rgb) {
    for (uint i = 0; i < 3; ++i) {
        if (rgb[i] <= 0.0031308) {
            rgb[i] *= 12.92;
        } else {
            rgb[i] = 1.055 * pow(rgb[i], 0.41666) - 0.055;
        }
    }
    return rgb;
}

vec3 uv_to_world(in mat4 inv_proj_view, in vec2 uv, in float depth) {
    const vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    const vec4 world = inv_proj_view * ndc;
    return world.xyz / world.w;
}

float sample_blue_noise_simple(in vec2 uv) {
    float v = 0.0;
    for (uint i = 0; i < 9; ++i) {
        v += IRIS_SIMPLE_HASH(uv + vec2(i % 3 - 1, i / 3 - 1));
    }
    return 0.9 * (1.125 * IRIS_SIMPLE_HASH(uv) - v / 8.0) + 0.5;
}

vec2 sample_hammersley(in uint i, in uint n) {
    const float s = 2.3283064365386963e-10;
    return vec2(
        float(i) / float(n),
        float(bitfieldReverse(i)) * s
    );
}

void iris_random_set_seed(uint x) {
    __iris_pcg_state = x;
}

uint iris_random() {
    __iris_pcg_state = __iris_pcg_state * 747796405u + 2891336453u;
    uint word = ((__iris_pcg_state >> ((__iris_pcg_state >> 28u) + 4u)) ^ __iris_pcg_state) * 277803737u;
    __iris_pcg_state = (word >> 22u) ^ word;
    return __iris_pcg_state;
}

float iris_random_float() {
    return iris_random() / float(0xffffffff);
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

#endif
