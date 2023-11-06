#pragma once

#include <utilities.hpp>

#include <glm/glm.hpp>

namespace test {
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
        auto update_aspect(float32 width, float32 height) noexcept -> void;

    private:
        //glm::vec3 _position = { 0.0f, 5.0f, 20.0f };
        glm::vec3 _position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 _front = { 0.0f, 0.0f, -1.0f };
        glm::vec3 _up = { 0.0f, 1.0f, 0.0f };
        glm::vec3 _right = { 1.0f, 0.0f, 0.0f };

        float32 _yaw = -90.0f;
        float32 _pitch = 0.0f;
        float32 _fov = 60.0f;
        float32 _near = 0.1f;
        float32 _far = 512.0f;

        float32 _width = 0.0f;
        float32 _height = 0.0f;

        std::reference_wrapper<ir::wsi_platform_t> _window;
    };

    auto make_perspective_frustum(const glm::mat4& pv) noexcept -> std::array<glm::vec4, 6>;
}