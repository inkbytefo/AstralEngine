#include "Camera.h"
#include "../../Core/Logger.h"

namespace AstralEngine {

Camera::Camera() 
    : m_isInitialized(false) {
    
    // Default konfigürasyon
    m_config = Config{};
    m_viewMatrix = glm::mat4(1.0f);
    m_projectionMatrix = glm::mat4(1.0f);
    
    Logger::Debug("Camera", "Camera created with default configuration");
}

Camera::Camera(const Config& config) 
    : m_config(config)
    , m_isInitialized(false) {
    
    m_viewMatrix = glm::mat4(1.0f);
    m_projectionMatrix = glm::mat4(1.0f);
    
    Logger::Debug("Camera", "Camera created with custom configuration");
}

void Camera::Initialize(const Config& config) {
    m_config = config;
    m_isInitialized = true;
    
    // Matrisleri hesapla
    UpdateMatrices();
    
    Logger::Info("Camera", "Camera initialized successfully");
    Logger::Debug("Camera", "Position: ({}, {}, {})", m_config.position.x, m_config.position.y, m_config.position.z);
    Logger::Debug("Camera", "Target: ({}, {}, {})", m_config.target.x, m_config.target.y, m_config.target.z);
    Logger::Debug("Camera", "FOV: {}°, Aspect Ratio: {}", m_config.fov, m_config.aspectRatio);
}

void Camera::Shutdown() {
    m_isInitialized = false;
    Logger::Info("Camera", "Camera shutdown completed");
}

void Camera::SetPosition(const glm::vec3& position) {
    m_config.position = position;
    UpdateViewMatrix();
    Logger::Debug("Camera", "Position updated to ({}, {}, {})", position.x, position.y, position.z);
}

void Camera::SetTarget(const glm::vec3& target) {
    m_config.target = target;
    UpdateViewMatrix();
    Logger::Debug("Camera", "Target updated to ({}, {}, {})", target.x, target.y, target.z);
}

void Camera::SetUp(const glm::vec3& up) {
    m_config.up = up;
    UpdateViewMatrix();
    Logger::Debug("Camera", "Up vector updated to ({}, {}, {})", up.x, up.y, up.z);
}

void Camera::SetFOV(float fov) {
    m_config.fov = fov;
    UpdateProjectionMatrix();
    Logger::Debug("Camera", "FOV updated to {}°", fov);
}

void Camera::SetAspectRatio(float aspectRatio) {
    m_config.aspectRatio = aspectRatio;
    UpdateProjectionMatrix();
    Logger::Debug("Camera", "Aspect ratio updated to {}", aspectRatio);
}

void Camera::SetNearPlane(float nearPlane) {
    m_config.nearPlane = nearPlane;
    UpdateProjectionMatrix();
    Logger::Debug("Camera", "Near plane updated to {}", nearPlane);
}

void Camera::SetFarPlane(float farPlane) {
    m_config.farPlane = farPlane;
    UpdateProjectionMatrix();
    Logger::Debug("Camera", "Far plane updated to {}", farPlane);
}

void Camera::SetLookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
    m_config.position = position;
    m_config.target = target;
    m_config.up = up;
    UpdateViewMatrix();
    Logger::Debug("Camera", "LookAt set to position({}, {}, {}), target({}, {}, {})", 
                 position.x, position.y, position.z, target.x, target.y, target.z);
}

void Camera::UpdateViewMatrix() {
    if (!m_isInitialized) {
        Logger::Warning("Camera", "Cannot update view matrix - camera not initialized");
        return;
    }
    
    m_viewMatrix = glm::lookAt(m_config.position, m_config.target, m_config.up);
    Logger::Debug("Camera", "View matrix updated");
}

void Camera::UpdateProjectionMatrix() {
    if (!m_isInitialized) {
        Logger::Warning("Camera", "Cannot update projection matrix - camera not initialized");
        return;
    }
    
    // FOV'yu radyana çevir
    float fovRadians = glm::radians(m_config.fov);
    
    m_projectionMatrix = glm::perspective(fovRadians, m_config.aspectRatio, m_config.nearPlane, m_config.farPlane);
    
    // Vulkan'ın [0, 1] derinlik aralığı için Y eksenini ters çevir
    m_projectionMatrix[1][1] *= -1;
    
    Logger::Debug("Camera", "Projection matrix updated for Vulkan clip space");
}

void Camera::UpdateMatrices() {
    UpdateViewMatrix();
    UpdateProjectionMatrix();
    Logger::Debug("Camera", "Both view and projection matrices updated");
}

void Camera::LookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
    SetLookAt(position, target, up);
}

void Camera::SetPerspective(float fov, float aspectRatio, float nearPlane, float farPlane) {
    m_config.fov = fov;
    m_config.aspectRatio = aspectRatio;
    m_config.nearPlane = nearPlane;
    m_config.farPlane = farPlane;
    UpdateProjectionMatrix();
    Logger::Debug("Camera", "Perspective set to fov={}°, aspect={}, near={}, far={}", 
                 fov, aspectRatio, nearPlane, farPlane);
}

} // namespace AstralEngine
