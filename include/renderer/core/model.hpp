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
        alignas(alignof(float32)) glm::vec4 tangent = {};
    };

    struct aabb_t {
        alignas(alignof(float32)) glm::vec3 min = {};
        alignas(alignof(float32)) glm::vec3 max = {};
    };

    struct meshlet_t {
        uint32 index_offset = 0;
        uint32 index_count = 0;
        uint32 primitive_offset = 0;
        uint32 primitive_count = 0;
        aabb_t aabb = {};
    };

    struct meshlet_group_t {
        uint32 vertex_offset = 0;
        uint32 vertex_count = 0;
        uint32 index_count = 0;

        std::vector<meshlet_t> meshlets;
    };

    class meshlet_model_t {
    public:
        using self = meshlet_model_t;

        IR_NODISCARD static auto make(const fs::path& path) noexcept -> self;

        IR_NODISCARD auto meshlet_groups() const noexcept -> std::span<const meshlet_group_t>;
        IR_NODISCARD auto vertices() const noexcept -> std::span<const meshlet_vertex_format_t>;
        IR_NODISCARD auto indices() const noexcept -> std::span<const uint32>;
        IR_NODISCARD auto primitives() const noexcept -> std::span<const uint8>;
        IR_NODISCARD auto transforms() const noexcept -> std::span<const glm::mat4>;
        IR_NODISCARD auto meshlet_count() const noexcept -> uint32;

    private:
        std::vector<meshlet_group_t> _meshlet_groups;
        std::vector<meshlet_vertex_format_t> _vertices;
        std::vector<uint32> _indices;
        std::vector<uint8> _primitives;

        std::vector<glm::mat4> _transforms;
        uint32 _meshlet_count = 0;
    };
}
