#pragma once

#include <renderer/core/utilities.hpp>

#include <glm/glm.hpp>

#include <cgltf.h>

#include <vector>
#include <span>

namespace app {
    struct meshlet_vertex_format_t {
        alignas(alignof(float32)) glm::vec3 position = {};
        alignas(alignof(float32)) glm::vec3 normal = {};
        alignas(alignof(float32)) glm::vec2 uv = {};
    };

    struct aabb_t {
        alignas(alignof(float32)) glm::vec3 min = {};
        alignas(alignof(float32)) glm::vec3 max = {};
    };

    struct meshlet_material_t {
        uint32 base_color_texture = -1;
        uint32 normal_texture = -1;
    };

    struct meshlet_t {
        uint32 id = 0;
        uint32 vertex_offset = 0;
        uint32 index_offset = 0;
        uint32 index_count = 0;
        uint32 primitive_offset = 0;
        uint32 primitive_count = 0;
        aabb_t aabb = {};
        meshlet_material_t material = {};
    };

    struct meshlet_instance_t {
        uint32 meshlet_id = 0;
        uint32 instance_id = 0;
    };

    enum class texture_type_t {
        e_base_color,
        e_normal,
        e_specular,
    };

    struct texture_info_t {
        uint32 id = 0;
        texture_type_t type = {};
        std::vector<uint8> data;
    };

    class meshlet_model_t {
    public:
        using self = meshlet_model_t;

        IR_NODISCARD static auto make(const fs::path& path) noexcept -> self;

        IR_NODISCARD auto meshlets() const noexcept -> std::span<const meshlet_t>;
        IR_NODISCARD auto meshlet_instances() const noexcept -> std::span<const meshlet_instance_t>;
        IR_NODISCARD auto vertices() const noexcept -> std::span<const meshlet_vertex_format_t>;
        IR_NODISCARD auto indices() const noexcept -> std::span<const uint32>;
        IR_NODISCARD auto primitives() const noexcept -> std::span<const uint8>;
        IR_NODISCARD auto transforms() const noexcept -> std::span<const glm::mat4>;
        IR_NODISCARD auto textures() const noexcept -> std::span<const texture_info_t>;
        IR_NODISCARD auto meshlet_count() const noexcept -> uint32;

    private:
        std::vector<meshlet_t> _meshlets;
        std::vector<meshlet_instance_t> _meshlet_instances;
        std::vector<meshlet_vertex_format_t> _vertices;
        std::vector<uint32> _indices;
        std::vector<uint8> _primitives;

        std::vector<texture_info_t> _textures;
        std::vector<glm::mat4> _transforms;
        uint32 _meshlet_count = 0;
    };
}
