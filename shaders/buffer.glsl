#ifndef IRIS_BUFFER_HEADER
#define IRIS_BUFFER_HEADER

#extension GL_EXT_shader_explicit_arithmetic_types : enable

#include "data.glsl"

#define IRIS_MAX_SHADOW_VIEWS 64

#define IRIS_MAIN_VIEW_INDEX 0
#define IRIS_SHADOW_VIEW_START_INDEX 1
#define IRIS_SHADOW_VIEW_END_INDEX (IRIS_MAX_SHADOW_VIEWS + IRIS_SHADOW_VIEW_START_INDEX)

layout (scalar, buffer_reference) restrict buffer b_view_block {
    view_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_meshlet_instance_block {
    meshlet_instance_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_meshlet_block {
    meshlet_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_transform_block {
    transform_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_vertex_block {
    vertex_format_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_index_block {
    uint[] data;
};

layout (scalar, buffer_reference) restrict buffer b_primitive_block {
    uint8_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_atomic_block {
    uint[] data;
};

layout (scalar, buffer_reference) restrict buffer b_material_block {
    material_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_directional_light_block {
    directional_light_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_vsm_page_request_block {
    uint8_t[] data;
};

layout (scalar, buffer_reference) restrict buffer b_vsm_globals_block {
    vsm_global_data_t data;
};

layout (scalar, buffer_reference) restrict buffer b_vsm_physical_page_table_block {
    uint[] data;
};

layout (scalar, buffer_reference) restrict buffer b_vsm_virtual_page_table_block {
    uint[] data;
};

layout (scalar, buffer_reference) restrict buffer b_vsm_allocate_request_block {
    uint count;
    uint[] data;
};


#endif
