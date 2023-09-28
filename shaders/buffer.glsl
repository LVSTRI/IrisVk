#ifndef IRIS_GLSL_BINDINGS_HEADER
#define IRIS_GLSL_BINDINGS_HEADER

#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "data.glsl"

#define IRIS_GLSL_MAX_SHADOW_VIEWS 64

#define IRIS_GLSL_MAIN_VIEW_INDEX 0
#define IRIS_GLSL_SHADOW_VIEW_START_INDEX 1
#define IRIS_GLSL_SHADOW_VIEW_END_INDEX (IRIS_GLSL_MAX_SHADOW_VIEWS + IRIS_GLSL_SHADOW_VIEW_START_INDEX)

#define IRIS_GLSL_VIEW_BUFFER_SLOT 0
#define IRIS_GLSL_MESHLET_INSTANCE_BUFFER_SLOT 1
#define IRIS_GLSL_MESHLET_BUFFER_SLOT 2
#define IRIS_GLSL_TRANSFORM_BUFFER_SLOT 3
#define IRIS_GLSL_VERTEX_BUFFER_SLOT 4
#define IRIS_GLSL_INDEX_BUFFER_SLOT 5
#define IRIS_GLSL_PRIMITIVE_BUFFER_SLOT 6
#define IRIS_GLSL_ATOMIC_BUFFER_SLOT 7

#define IRIS_GLSL_VIEW_BUFFER_BLOCK(name) name { \
    view_t[] data;                               \
}

#define IRIS_GLSL_MESHLET_INSTANCE_BUFFER_BLOCK(name) name { \
    meshlet_instance_t[] data;                               \
}

#define IRIS_GLSL_MESHLET_BUFFER_BLOCK(name) name { \
    meshlet_t[] data;                               \
}

#define IRIS_GLSL_TRANSFORM_BUFFER_BLOCK(name) name { \
    transform_t[] data;                               \
}

#define IRIS_GLSL_VERTEX_BUFFER_BLOCK(name) name { \
    vertex_format_t[] data;                        \
}

#define IRIS_GLSL_INDEX_BUFFER_BLOCK(name) name { \
    uint[] data;                                  \
}

#define IRIS_GLSL_PRIMITIVE_BUFFER_BLOCK(name) name { \
    uint8_t[] data;                                   \
}

#define IRIS_GLSL_ADDRESS_BUFFER_BLOCK(name) name { \
    uint64_t[] data;                                \
}

#define IRIS_GLSL_ATOMIC_BUFFER_BLOCK(name) name { \
    uint[] data;                                   \
}

#endif
