#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Bounds.h"

namespace AstralEngine {

/**
 * @class Camera
 * @brief 3D kamera sınıfı - view ve projection matrislerini üretir
 * 
 * Bu sınıf, 3D sahnelerde kamera konumunu, yönünü ve projeksiyonunu
 * yönetmek için kullanılır. View matrisi (kamera dönüşümü) ve
 * projection matrisi (perspektif projeksiyon) üretir.
 */
class Camera {
public:
    /**
     * @brief Camera yapılandırma parametreleri
     */
    struct Config {
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 2.0f);    ///< Kamera pozisyonu
        glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);      ///< Bakılan hedef noktası
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);          ///< Yukarı vektörü
        float fov = 45.0f;                                      ///< Field of view (derece)
        float aspectRatio = 16.0f / 9.0f;                      ///< En-boy oranı
        float nearPlane = 0.1f;                                 ///< Yakın kesme düzlemi
        float farPlane = 100.0f;                                ///< Uzak kesme düzlemi
    };

    Camera();
    explicit Camera(const Config& config);

    // Non-copyable
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    // Yaşam döngüsü
    void Initialize(const Config& config);
    void Shutdown();

    // Getter'lar
    const glm::mat4& GetViewMatrix();
    const glm::mat4& GetProjectionMatrix();
    const Frustum& GetFrustum();
    
    // Const getter'lar
    const glm::mat4& GetViewMatrix() const;
    const glm::mat4& GetProjectionMatrix() const;
    const Frustum& GetFrustum() const;
    const glm::vec3& GetPosition() const { return m_config.position; }
    const glm::vec3& GetTarget() const { return m_config.target; }
    const glm::vec3& GetUp() const { return m_config.up; }
    float GetFOV() const { return m_config.fov; }
    float GetAspectRatio() const { return m_config.aspectRatio; }
    float GetNearPlane() const { return m_config.nearPlane; }
    float GetFarPlane() const { return m_config.farPlane; }

    // Setter'lar
    void SetPosition(const glm::vec3& position);
    void SetTarget(const glm::vec3& target);
    void SetUp(const glm::vec3& up);
    void SetFOV(float fov);
    void SetAspectRatio(float aspectRatio);
    void SetNearPlane(float nearPlane);
    void SetFarPlane(float farPlane);
    void SetLookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up);

    // Yardımcı fonksiyonlar
    void LookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up);
    void SetPerspective(float fov, float aspectRatio, float nearPlane, float farPlane);

private:
    void UpdateViewMatrixInternal() const;
    void UpdateProjectionMatrixInternal() const;
    void ExtractFrustumPlanesInternal() const;

    Config m_config;
    mutable glm::mat4 m_viewMatrix;
    mutable glm::mat4 m_projectionMatrix;
    mutable Frustum m_frustum;
    mutable bool m_viewDirty = true;
    mutable bool m_projectionDirty = true;
    mutable bool m_frustumDirty = true;
    bool m_isInitialized;
};

} // namespace AstralEngine