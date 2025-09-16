#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

namespace AstralEngine {

/**
 * @brief Transform bileşeni - Her entity'nin pozisyon, rotasyon ve ölçek bilgisi
 */
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // Euler angles in radians
    glm::vec3 scale{1.0f};

    // World matrix hesaplama
    glm::mat4 GetWorldMatrix() const {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 scaling = glm::scale(glm::mat4(1.0f), scale);
        
        return translation * rotY * rotX * rotZ * scaling;
    }
};

/**
 * @brief Render bileşeni - Görsellik ile ilgili bilgiler
 */
struct RenderComponent {
    std::string modelPath;
    std::string materialPath;
    bool visible = true;
    int renderLayer = 0; // Lower values render first
    
    // Render özellikleri
    bool castsShadows = true;
    bool receivesShadows = true;
    
    RenderComponent() = default;
    RenderComponent(const std::string& model, const std::string& material)
        : modelPath(model), materialPath(material) {}
};

/**
 * @brief Name bileşeni - Entity'nin ismi
 */
struct NameComponent {
    std::string name;
    
    NameComponent() = default;
    NameComponent(const std::string& n) : name(n) {}
};

/**
 * @brief Tag bileşeni - Entity'ye etiket vermek için
 */
struct TagComponent {
    std::string tag;
    
    TagComponent() = default;
    TagComponent(const std::string& t) : tag(t) {}
};

/**
 * @brief Movement bileşeni - Hareket ile ilgili veriler
 */
struct MovementComponent {
    glm::vec3 velocity{0.0f};
    glm::vec3 acceleration{0.0f};
    glm::vec3 angularVelocity{0.0f};
    
    float maxSpeed = 10.0f;
    float friction = 0.98f; // 0.0f = no friction, 1.0f = instant stop
    
    MovementComponent() = default;
    MovementComponent(float maxSpd) : maxSpeed(maxSpd) {}
};

/**
 * @brief Camera bileşeni - Kamera özellikleri
 */
struct CameraComponent {
    enum class ProjectionType { Perspective, Orthographic };
    
    ProjectionType projectionType = ProjectionType::Perspective;
    
    // Perspective projection
    float fieldOfView = 45.0f; // degrees
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    
    // Orthographic projection
    float orthoLeft = -10.0f;
    float orthoRight = 10.0f;
    float orthoBottom = -10.0f;
    float orthoTop = 10.0f;
    
    bool isMainCamera = false;
    
    // Projection matrix hesaplama
    glm::mat4 GetProjectionMatrix(float aspectRatio) const {
        if (projectionType == ProjectionType::Perspective) {
            return glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
        } else {
            return glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, nearPlane, farPlane);
        }
    }
};

/**
 * @brief Light bileşeni - Işık kaynakları için
 */
struct LightComponent {
    enum class LightType { Directional, Point, Spot };
    
    LightType type = LightType::Point;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    
    // Point ve Spot light için
    float range = 10.0f;
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
    
    // Spot light için
    float innerCone = 30.0f; // degrees
    float outerCone = 45.0f; // degrees
    
    // Shadow casting
    bool castsShadows = false;
    
    LightComponent() = default;
    LightComponent(LightType t, const glm::vec3& col, float intens)
        : type(t), color(col), intensity(intens) {}
};

/**
 * @brief Script bileşeni - Entity'ye script bağlamak için
 */
struct ScriptComponent {
    std::string scriptPath;
    bool enabled = true;
    
    // Script verilerini tutmak için generic container
    // Gerçek implementasyon script system'e bağlı olacak
    void* scriptInstance = nullptr;
    
    ScriptComponent() = default;
    ScriptComponent(const std::string& path) : scriptPath(path) {}
};

/**
 * @brief Hierarchy bileşeni - Entity hiyerarşisi için
 */
struct HierarchyComponent {
    uint32_t parent = 0;  // 0 = no parent
    std::vector<uint32_t> children;
    
    HierarchyComponent() = default;
    HierarchyComponent(uint32_t parentId) : parent(parentId) {}
};

} // namespace AstralEngine
