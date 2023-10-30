#include <camera.hpp>

#include <iris/wsi/wsi_platform.hpp>
#include <iris/wsi/input.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace test {
    IR_NODISCARD static auto plane_from_points(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) noexcept -> glm::vec4 {
        IR_PROFILE_SCOPED();
        auto plane = glm::vec4();
        plane = glm::make_vec4(glm::normalize(glm::cross(c - a, b - a)));
        plane.w = glm::dot(glm::vec3(plane), a);
        return plane;
    }

    camera_t::camera_t(ir::wsi_platform_t& window) noexcept
        : _window(std::ref(window)) {
        IR_PROFILE_SCOPED();
    }

    auto camera_t::make(ir::wsi_platform_t& window) noexcept -> self {
        IR_PROFILE_SCOPED();
        return self(window);
    }

    auto camera_t::position() const noexcept -> const glm::vec3& {
        IR_PROFILE_SCOPED();
        return _position;
    }

    auto camera_t::front() const noexcept -> const glm::vec3& {
        IR_PROFILE_SCOPED();
        return _front;
    }

    auto camera_t::up() const noexcept -> const glm::vec3& {
        IR_PROFILE_SCOPED();
        return _up;
    }

    auto camera_t::right() const noexcept -> const glm::vec3& {
        IR_PROFILE_SCOPED();
        return _right;
    }

    auto camera_t::yaw() const noexcept -> float32 {
        IR_PROFILE_SCOPED();
        return _yaw;
    }

    auto camera_t::pitch() const noexcept -> float32 {
        IR_PROFILE_SCOPED();
        return _pitch;
    }

    auto camera_t::fov() const noexcept -> float32 {
        IR_PROFILE_SCOPED();
        return glm::radians(_fov);
    }

    auto camera_t::aspect() const noexcept -> float32 {
        IR_PROFILE_SCOPED();
        return _width / _height;
    }

    auto camera_t::near() const noexcept -> float32 {
        IR_PROFILE_SCOPED();
        return _near;
    }

    auto camera_t::far() const noexcept -> float32 {
        IR_PROFILE_SCOPED();
        return _far;
    }

    auto camera_t::view() const noexcept -> glm::mat4 {
        IR_PROFILE_SCOPED();
        return glm::lookAt(_position, _position + _front, _up);
    }

    auto camera_t::projection(bool infinite, bool reverse_z) const noexcept -> glm::mat4 {
        IR_PROFILE_SCOPED();
        const auto type = ((uint32(infinite) << 1u) | uint32(reverse_z));
        auto p = glm::mat4();
        switch (type) {
            case 0b00u:
                p = glm::perspective(fov(), aspect(), _near, _far);
                break;
            case 0b01u:
                p = glm::perspective(fov(), aspect(), _far, _near);
                break;
            case 0b10u:
                p = glm::infinitePerspective(fov(), aspect(), _near);
                break;
            case 0b11u:
                const auto f = 1.0f / glm::tan(fov() / 2.0f);
                p = {
                    f / aspect(), 0.0f, 0.0f, 0.0f,
                    0.0f, f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, -1.0f,
                    0.0f, 0.0f, _near, 0.0f
                };
                break;
        }
        p[1][1] *= -1;
        return p;
    }

    auto camera_t::update(float32 dt) noexcept -> void {
        IR_PROFILE_SCOPED();
        constexpr static auto sensitivity = 0.1f;
        auto& window = _window.get();
        const auto speed = 5.0f * dt;

        const auto& [dx, dy] = window.input().cursor_delta();
        _yaw += sensitivity * dx;
        _pitch += sensitivity * dy;
        if (_pitch > 89.9f) {
            _pitch = 89.9f;
        }
        if (_pitch < -89.9f) {
            _pitch = -89.9f;
        }
        const auto r_yaw = glm::radians(_yaw);
        const auto r_pitch = glm::radians(_pitch);

        if (window.input().is_pressed(ir::keyboard_t::e_w)) {
            _position.x += glm::cos(r_yaw) * speed;
            _position.z += glm::sin(r_yaw) * speed;
        }
        if (window.input().is_pressed(ir::keyboard_t::e_s)) {
            _position.x -= glm::cos(r_yaw) * speed;
            _position.z -= glm::sin(r_yaw) * speed;
        }
        if (window.input().is_pressed(ir::keyboard_t::e_d)) {
            _position += speed * _right;
        }
        if (window.input().is_pressed(ir::keyboard_t::e_a)) {
            _position -= speed * _right;
        }
        if (window.input().is_pressed(ir::keyboard_t::e_space)) {
            _position.y += speed;
        }
        if (window.input().is_pressed(ir::keyboard_t::e_left_shift)) {
            _position.y -= speed;
        }

        _front = glm::normalize(glm::vec3(
            glm::cos(r_pitch) * glm::cos(r_yaw),
            glm::sin(r_pitch),
            glm::cos(r_pitch) * glm::sin(r_yaw)));
        _right = glm::normalize(glm::cross(_front, { 0.0f, 1.0f, 0.0f }));
        _up = glm::normalize(glm::cross(_right, _front));
    }

    auto camera_t::update_aspect(float32 width, float32 height) noexcept -> void {
        IR_PROFILE_SCOPED();
        _width = width;
        _height = height;
    }

    IR_NODISCARD auto make_perspective_frustum(const glm::mat4& pv) noexcept -> std::array<glm::vec4, 6> {
        IR_PROFILE_SCOPED();
        auto planes = std::array<glm::vec4, 6>();
        for (auto i = 0_u32; i < 4; ++i) { planes[0][i] = pv[i][3] + pv[i][0]; }
        for (auto i = 0_u32; i < 4; ++i) { planes[1][i] = pv[i][3] - pv[i][0]; }
        for (auto i = 0_u32; i < 4; ++i) { planes[2][i] = pv[i][3] + pv[i][1]; }
        for (auto i = 0_u32; i < 4; ++i) { planes[3][i] = pv[i][3] - pv[i][1]; }
        for (auto i = 0_u32; i < 4; ++i) { planes[4][i] = pv[i][3] + pv[i][2]; }
        for (auto i = 0_u32; i < 4; ++i) { planes[5][i] = pv[i][3] - pv[i][2]; }

        for (auto i = 0_u32; i < 6; ++i) {
            planes[i] /= glm::length(glm::vec3(planes[i]));
            planes[i].w = -planes[i].w;
        }
        return planes;
    }
}
