#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace AstralEngine {

enum class CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class Camera {
public:
    // Constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

    // Returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix() const;

    // Returns the projection matrix
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;

    // Processes input received from any keyboard-like input system
    void ProcessKeyboard(CameraMovement direction, float deltaTime);

    // Processes input received from a mouse input system
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

    // Processes input received from a mouse scroll-wheel event
    void ProcessMouseScroll(float yoffset);

    // Setters
    void SetPosition(const glm::vec3& position) { m_position = position; }
    void SetLookAt(const glm::vec3& target);
    void SetMovementSpeed(float speed) { m_movementSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { m_mouseSensitivity = sensitivity; }
    void SetZoom(float zoom) { m_zoom = zoom; }

    // Getters
    glm::vec3 GetPosition() const { return m_position; }
    float GetZoom() const { return m_zoom; }
    glm::vec3 GetFront() const { return m_front; }
    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }

private:
    // Calculates the front vector from the Camera's (updated) Euler Angles
    void UpdateCameraVectors();

    // Camera Attributes
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    // Euler Angles
    float m_yaw;
    float m_pitch;

    // Camera Options
    float m_movementSpeed = 2.5f;
    float m_mouseSensitivity = 0.1f;
    float m_zoom = 45.0f;
    
    // Projection settings
    float m_nearPlane = 0.1f;
    float m_farPlane = 100.0f;
};

} // namespace AstralEngine
