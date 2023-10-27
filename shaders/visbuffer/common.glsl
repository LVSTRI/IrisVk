#ifndef IRIS_GLSL_VISBUFFER_COMMON_HEADER
#define IRIS_GLSL_VISBUFFER_COMMON_HEADER

#define CLUSTER_MAX_VERTICES 64
#define CLUSTER_MAX_PRIMITIVES 64

#define CLUSTER_ID_BITS 26
#define CLUSTER_ID_MASK ((1 << CLUSTER_ID_BITS) - 1)
#define CLUSTER_PRIMITIVE_ID_BITS 6
#define CLUSTER_PRIMITIVE_ID_MASK ((1 << CLUSTER_PRIMITIVE_ID_BITS) - 1)

#endif