#ifndef IRIS_VISBUFFER_COMMON_HEADER
#define IRIS_VISBUFFER_COMMON_HEADER

#define CLUSTER_MAX_VERTICES 64
#define CLUSTER_MAX_PRIMITIVES 64

#define CLUSTER_ID_BITS 26
#define CLUSTER_ID_MASK ((1 << CLUSTER_ID_BITS) - 1)
#define CLUSTER_PRIMITIVE_ID_BITS 6
#define CLUSTER_PRIMITIVE_ID_MASK ((1 << CLUSTER_PRIMITIVE_ID_BITS) - 1)

struct partial_derivatives_t {
    vec3 lambda;
    vec3 ddx;
    vec3 ddy;
};

struct gradient_vec2_t {
    vec2 lambda;
    vec2 ddx;
    vec2 ddy;
};

struct gradient_vec3_t {
    vec3 lambda;
    vec3 ddx;
    vec3 ddy;
};

partial_derivatives_t compute_derivatives(in vec4[3] clip_position, in vec2 ndc_uv, in vec2 resolution) {
    partial_derivatives_t result;
    const vec3 inv_w = 1.0 / vec3(clip_position[0].w, clip_position[1].w, clip_position[2].w);
    const vec2 ndc_0 = clip_position[0].xy * inv_w[0];
    const vec2 ndc_1 = clip_position[1].xy * inv_w[1];
    const vec2 ndc_2 = clip_position[2].xy * inv_w[2];

    const float inv_det = 1.0 / determinant(mat2(ndc_2 - ndc_1, ndc_0 - ndc_1));
    result.ddx = vec3(ndc_1.y - ndc_2.y, ndc_2.y - ndc_0.y, ndc_0.y - ndc_1.y) * inv_det * inv_w;
    result.ddy = vec3(ndc_2.x - ndc_1.x, ndc_0.x - ndc_2.x, ndc_1.x - ndc_0.x) * inv_det * inv_w;

    float ddx_sum = dot(result.ddx, vec3(1.0));
    float ddy_sum = dot(result.ddy, vec3(1.0));

    const vec2 delta_v = ndc_uv - ndc_0;
    const float interp_inv_w = inv_w.x + delta_v.x * ddx_sum + delta_v.y * ddy_sum;
    const float interp_w = 1.0 / interp_inv_w;

    result.lambda = vec3(
        interp_w * (delta_v.x * result.ddx.x + delta_v.y * result.ddy.x + inv_w.x),
        interp_w * (delta_v.x * result.ddx.y + delta_v.y * result.ddy.y),
        interp_w * (delta_v.x * result.ddx.z + delta_v.y * result.ddy.z));

    result.ddx *= 2.0 / resolution.x;
    result.ddy *= -2.0 / resolution.y;
    ddx_sum *= 2.0 / resolution.x;
    ddy_sum *= -2.0 / resolution.y;

    const float interp_w_ddx = 1.0 / (interp_inv_w.x + ddx_sum);
    const float interp_w_ddy = 1.0 / (interp_inv_w.x + ddy_sum);

    result.ddx = interp_w_ddx * (result.lambda * interp_inv_w + result.ddx) - result.lambda;
    result.ddy = interp_w_ddy * (result.lambda * interp_inv_w + result.ddy) - result.lambda;
    return result;
}

vec3 interpolate_with_derivatives(in partial_derivatives_t derivatives, in vec3 values) {
    return vec3(
        dot(values, derivatives.lambda),
        dot(values, derivatives.ddx),
        dot(values, derivatives.ddy)
    );
}

vec3 interpolate(in partial_derivatives_t derivatives, in vec3[3] values) {
    return
        derivatives.lambda.x * values[0] +
        derivatives.lambda.y * values[1] +
        derivatives.lambda.z * values[2];
}

vec4 interpolate(in partial_derivatives_t derivatives, in vec4[3] values) {
    return
        derivatives.lambda.x * values[0] +
        derivatives.lambda.y * values[1] +
        derivatives.lambda.z * values[2];
}

gradient_vec2_t make_gradient_vec2(in partial_derivatives_t derivatives, in vec2[3] values) {
    const vec3 v0 = interpolate_with_derivatives(derivatives, vec3(values[0].x, values[1].x, values[2].x));
    const vec3 v1 = interpolate_with_derivatives(derivatives, vec3(values[0].y, values[1].y, values[2].y));
    return gradient_vec2_t(
        vec2(v0.x, v1.x),
        vec2(v0.y, v1.y),
        vec2(v0.z, v1.z)
    );
}

gradient_vec3_t make_gradient_vec3(in partial_derivatives_t derivatives, in vec3[3] values) {
    const vec3 v0 = interpolate_with_derivatives(derivatives, vec3(values[0].x, values[1].x, values[2].x));
    const vec3 v1 = interpolate_with_derivatives(derivatives, vec3(values[0].y, values[1].y, values[2].y));
    const vec3 v2 = interpolate_with_derivatives(derivatives, vec3(values[0].z, values[1].z, values[2].z));
    return gradient_vec3_t(
        vec3(v0.x, v1.x, v2.x),
        vec3(v0.y, v1.y, v2.y),
        vec3(v0.z, v1.z, v2.z)
    );
}

#endif
