#include <renderer/core/model.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <meshoptimizer.h>

#include <filesystem>
#include <cstring>
#include <queue>
#include <vector>
#include <array>
#include <span>

namespace app {
    static auto decode_texture_path(const fs::path& base, const cgltf_image& image) noexcept {
        if (image.uri) {
            return (base / image.uri).generic_string();
        }

        auto path = base / image.name;
        if (!path.has_extension()) {
            if (std::strcmp(image.mime_type, "image/png")) {
                path.replace_extension(".png");
            } else if (std::strcmp(image.mime_type, "image/jpeg")) {
                path.replace_extension(".jpg");
            }
        }
        return path.generic_string();
    }

    static auto is_texture_valid(const cgltf_texture* texture) noexcept {
        if (texture) {
            if (texture->image) {
                return
                    texture->image->buffer_view &&
                    texture->image->buffer_view->buffer;
            }
            return
                texture->basisu_image->buffer_view &&
                texture->basisu_image->buffer_view->buffer;
        }
        return false;
    }

    auto meshlet_model_t::make(const fs::path& path) noexcept -> self {
        auto model = self();
        auto options = cgltf_options();
        auto* gltf = (cgltf_data*)(nullptr);
        const auto s_path = path.generic_string();
        cgltf_parse_file(&options, s_path.c_str(), &gltf);
        cgltf_load_buffers(&options, gltf, s_path.c_str());

        auto vertex_offset = 0_u32;
        auto index_offset = 0_u32;
        auto primitive_offset = 0_u32;
        auto total_meshlets = 0_u32;
        for (auto i = 0_u32; i < gltf->scene->nodes_count; ++i) {
            auto nodes = std::queue<const cgltf_node*>();
            nodes.push(gltf->scene->nodes[i]);
            while (!nodes.empty()) {
                const auto& node = *nodes.front();
                nodes.pop();
                for (auto j = 0_u32; j < node.children_count; ++j) {
                    nodes.push(node.children[j]);
                }
                if (!node.mesh) {
                    continue;
                }
                auto transform = glm::identity<glm::mat4>();
                cgltf_node_transform_world(&node, glm::value_ptr(transform));

                const auto& mesh = *node.mesh;
                for (auto j = 0_u32; j < mesh.primitives_count; ++j) {
                    const auto& primitive = mesh.primitives[j];
                    const auto* position_ptr = (glm::vec3*)(nullptr);
                    const auto* normal_ptr = (glm::vec3*)(nullptr);
                    const auto* uv_ptr = (glm::vec2*)(nullptr);
                    const auto* tangent_ptr = (glm::vec4*)(nullptr);

                    auto vertices = std::vector<meshlet_vertex_format_t>();
                    auto vertex_count = 0_u32;
                    for (auto k = 0_u32; k < primitive.attributes_count; ++k) {
                        const auto& attribute = primitive.attributes[k];
                        const auto& accessor = *attribute.data;
                        const auto& buffer_view = *accessor.buffer_view;
                        const auto& buffer = *buffer_view.buffer;
                        const auto& data_ptr = static_cast<const char*>(buffer.data);
                        switch (attribute.type) {
                            case cgltf_attribute_type_position:
                                vertex_count = accessor.count;
                                position_ptr = reinterpret_cast<const glm::vec3*>(data_ptr + buffer_view.offset + accessor.offset);
                                break;

                            case cgltf_attribute_type_normal:
                                normal_ptr = reinterpret_cast<const glm::vec3*>(data_ptr + buffer_view.offset + accessor.offset);
                                break;

                            case cgltf_attribute_type_texcoord:
                                if (!uv_ptr) {
                                    uv_ptr = reinterpret_cast<const glm::vec2*>(data_ptr + buffer_view.offset + accessor.offset);
                                }
                                break;

                            case cgltf_attribute_type_tangent:
                                tangent_ptr = reinterpret_cast<const glm::vec4*>(data_ptr + buffer_view.offset + accessor.offset);
                                break;

                            default: break;
                        }
                    }
                    if (!vertex_count) {
                        continue;
                    }
                    vertices.resize(vertex_count);
                    for (auto k = 0_u32; k < vertex_count; ++k) {
                        auto& vertex = vertices[k];
                        std::memcpy(&vertex.position, &position_ptr[k], sizeof(vertex.position));
                        if (normal_ptr) {
                            std::memcpy(&vertex.normal, &normal_ptr[k], sizeof(vertex.normal));
                        }
                        if (uv_ptr) {
                            std::memcpy(&vertex.uv, &uv_ptr[k], sizeof(vertex.uv));
                        }
                        if (tangent_ptr) {
                            std::memcpy(&vertex.tangent, &tangent_ptr[k], sizeof(vertex.tangent));
                        }
                    }
                    auto indices = std::vector<uint32>();
                    {
                        const auto& accessor = *primitive.indices;
                        const auto& buffer_view = *accessor.buffer_view;
                        const auto& buffer = *buffer_view.buffer;
                        const auto& data_ptr = static_cast<const char*>(buffer.data);
                        indices.reserve(accessor.count);
                        switch (accessor.component_type) {
                            case cgltf_component_type_r_8:
                            case cgltf_component_type_r_8u: {
                                const auto* ptr = reinterpret_cast<const uint8*>(data_ptr + buffer_view.offset + accessor.offset);
                                std::ranges::copy(std::span(ptr, accessor.count), std::back_inserter(indices));
                            } break;

                            case cgltf_component_type_r_16:
                            case cgltf_component_type_r_16u: {
                                const auto* ptr = reinterpret_cast<const uint16*>(data_ptr + buffer_view.offset + accessor.offset);
                                std::ranges::copy(std::span(ptr, accessor.count), std::back_inserter(indices));
                            } break;

                            case cgltf_component_type_r_32f:
                            case cgltf_component_type_r_32u: {
                                const auto* ptr = reinterpret_cast<const uint32*>(data_ptr + buffer_view.offset + accessor.offset);
                                std::ranges::copy(std::span(ptr, accessor.count), std::back_inserter(indices));
                            } break;

                            default: break;
                        }
                    }

                    // optimization
                    auto optimized_indices = std::vector<uint32>();
                    auto optimized_vertices = std::vector<meshlet_vertex_format_t>();
                    {
                        const auto index_count = indices.size();
                        auto index_remap = std::vector<uint32>(index_count);
                        const auto remap_vertex_count = meshopt_generateVertexRemap(
                            index_remap.data(),
                            indices.data(),
                            index_count,
                            vertices.data(),
                            vertex_count,
                            sizeof(meshlet_vertex_format_t));
                        optimized_indices.resize(index_count);
                        optimized_vertices.resize(remap_vertex_count);
                        meshopt_remapIndexBuffer(
                            optimized_indices.data(),
                            indices.data(),
                            index_remap.size(),
                            index_remap.data());
                        meshopt_remapVertexBuffer(
                            optimized_vertices.data(),
                            vertices.data(),
                            remap_vertex_count,
                            sizeof(meshlet_vertex_format_t),
                            index_remap.data());
                        meshopt_optimizeVertexCache(
                            optimized_indices.data(),
                            optimized_indices.data(),
                            optimized_indices.size(),
                            optimized_indices.size());
                        meshopt_optimizeOverdraw(
                            optimized_indices.data(),
                            optimized_indices.data(),
                            optimized_indices.size(),
                            reinterpret_cast<float32*>(optimized_vertices.data()),
                            optimized_vertices.size(),
                            sizeof(meshlet_vertex_format_t),
                            1.05f);
                        meshopt_optimizeVertexFetch(
                            optimized_vertices.data(),
                            optimized_indices.data(),
                            optimized_indices.size(),
                            optimized_vertices.data(),
                            optimized_vertices.size(),
                            sizeof(meshlet_vertex_format_t));
                    }
                    // meshlet build
                    {
                        constexpr static auto max_indices = 40_u32;
                        constexpr static auto max_primitives = 84_u32;
                        constexpr static auto cone_weight = 0.0f;
                        const auto max_meshlets = meshopt_buildMeshletsBound(optimized_indices.size(), max_indices, max_primitives);
                        auto meshlets = std::vector<meshopt_Meshlet>(max_meshlets);
                        auto meshlet_indices = std::vector<uint32>(max_meshlets * max_indices);
                        auto meshlet_primitives = std::vector<uint8>(max_meshlets * max_primitives * 3);
                        const auto meshlet_count = meshopt_buildMeshlets(
                            meshlets.data(),
                            meshlet_indices.data(),
                            meshlet_primitives.data(),
                            optimized_indices.data(),
                            optimized_indices.size(),
                            reinterpret_cast<float32*>(optimized_vertices.data()),
                            optimized_vertices.size(),
                            sizeof(meshlet_vertex_format_t),
                            max_indices,
                            max_primitives,
                            cone_weight);
                        auto& last_meshlet = meshlets[meshlet_count - 1];
                        meshlet_indices.resize(last_meshlet.vertex_offset + last_meshlet.vertex_count);
                        meshlet_primitives.resize(last_meshlet.triangle_offset + ((last_meshlet.triangle_count * 3 + 3) & ~3));
                        meshlets.resize(meshlet_count);

                        auto meshlet_group = meshlet_group_t();
                        {
                            meshlet_group.vertex_count = optimized_vertices.size();
                            meshlet_group.vertex_offset = vertex_offset;
                            meshlet_group.index_count = meshlet_indices.size();
                            meshlet_group.meshlets.reserve(meshlet_count);
                            for (auto k = 0_u32; k < meshlet_count; ++k) {
                                auto& meshlet = meshlet_group.meshlets.emplace_back();
                                meshlet.index_offset = index_offset + meshlets[k].vertex_offset;
                                meshlet.index_count = meshlets[k].vertex_count;
                                meshlet.primitive_offset = primitive_offset + meshlets[k].triangle_offset;
                                meshlet.primitive_count = meshlets[k].triangle_count;

                                auto aabb = aabb_t();
                                aabb.min = glm::vec3(std::numeric_limits<float32>::max());
                                aabb.max = glm::vec3(std::numeric_limits<float32>::lowest());
                                for (auto z = 0_u32; z < meshlet.primitive_count * 3; ++z) {
                                    const auto offset = meshlet_indices[meshlets[k].vertex_offset +
                                        meshlet_primitives[meshlets[k].triangle_offset + z]];
                                    const auto& vertex = optimized_vertices[offset];
                                    aabb.min = glm::min(aabb.min, vertex.position);
                                    aabb.max = glm::max(aabb.max, vertex.position);
                                }
                                meshlet.aabb = aabb;
                            }
                        }
                        total_meshlets += meshlet_count;
                        vertex_offset += optimized_vertices.size();
                        index_offset += meshlet_indices.size();
                        primitive_offset += meshlet_primitives.size();

                        model._vertices.insert(model._vertices.end(), optimized_vertices.begin(), optimized_vertices.end());
                        model._indices.insert(model._indices.end(), meshlet_indices.begin(), meshlet_indices.end());
                        model._primitives.insert(model._primitives.end(), meshlet_primitives.begin(), meshlet_primitives.end());
                        model._meshlet_groups.emplace_back(std::move(meshlet_group));
                        model._transforms.emplace_back(transform);
                    }
                }
            }
        }
        model._meshlet_count = total_meshlets;
        return model;
    }

    auto meshlet_model_t::meshlet_groups() const noexcept -> std::span<const meshlet_group_t> {
        return _meshlet_groups;
    }

    auto meshlet_model_t::vertices() const noexcept -> std::span<const meshlet_vertex_format_t> {
        return _vertices;
    }

    auto meshlet_model_t::indices() const noexcept -> std::span<const uint32> {
        return _indices;
    }

    auto meshlet_model_t::primitives() const noexcept -> std::span<const uint8> {
        return _primitives;
    }

    auto meshlet_model_t::transforms() const noexcept -> std::span<const glm::mat4> {
        return _transforms;
    }

    auto meshlet_model_t::meshlet_count() const noexcept -> uint32 {
        return _meshlet_count;
    }
}
