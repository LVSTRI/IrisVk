#pragma once

#include <iris/core/types.hpp>
#include <iris/core/macros.hpp>
#include <iris/core/forwards.hpp>
#include <iris/core/enums.hpp>

#include <renderer/core/utilities.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace app {
    struct alignas(16) plane_t {
        plane_t() noexcept = default;
        plane_t(const glm::vec3& n, const glm::vec3& p) noexcept
            : normal(glm::normalize(n)),
              distance(glm::dot(normal, p)) {}

        glm::vec3 normal = {};
        float32 distance = 0;
    };

    struct frustum_t {
        plane_t planes[6] = {};
    };

    class camera_t {
    public:
        using self = camera_t;

        camera_t(ir::wsi_platform_t& window) noexcept;
        ~camera_t() noexcept = default;

        camera_t(const self&) noexcept = default;
        auto operator =(const self&) noexcept -> self& = default;
        camera_t(self&&) noexcept = default;
        auto operator =(self&&) noexcept -> self& = default;

        IR_NODISCARD static auto make(ir::wsi_platform_t& window) noexcept -> self;

        IR_NODISCARD auto position() const noexcept -> const glm::vec3&;
        IR_NODISCARD auto front() const noexcept -> const glm::vec3&;
        IR_NODISCARD auto up() const noexcept -> const glm::vec3&;
        IR_NODISCARD auto right() const noexcept -> const glm::vec3&;

        IR_NODISCARD auto yaw() const noexcept -> float32;
        IR_NODISCARD auto pitch() const noexcept -> float32;
        IR_NODISCARD auto fov() const noexcept -> float32;
        IR_NODISCARD auto aspect() const noexcept -> float32;
        IR_NODISCARD auto near() const noexcept -> float32;
        IR_NODISCARD auto far() const noexcept -> float32;

        IR_NODISCARD auto view() const noexcept -> glm::mat4;
        IR_NODISCARD auto projection(bool infinite = true, bool reverse_z = true) const noexcept -> glm::mat4;

        auto update(float32 dt) noexcept -> void;

    private:
        glm::vec3 _position = { 0.0f, 0.0f, 2.0f };
        glm::vec3 _front = { 0.0f, 0.0f, -1.0f };
        glm::vec3 _up = { 0.0f, 1.0f, 0.0f };
        glm::vec3 _right = { 1.0f, 0.0f, 0.0f };

        float32 _yaw = -90.0f;
        float32 _pitch = 0.0f;
        float32 _fov = 60.0f;
        float32 _near = 0.1f;
        float32 _far = 512.0f;

        std::reference_wrapper<ir::wsi_platform_t> _window;
    };

    auto make_perspective_frustum(const glm::mat4& pv) noexcept -> frustum_t;
}
