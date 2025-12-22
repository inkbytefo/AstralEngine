#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <memory>
#include "../Subsystems/Asset/AssetHandle.h"
#include "../Core/UUID.h"
#include <entt/entt.hpp>
#include "../Core/MathUtils.h" // Added

namespace AstralEngine {

struct IDComponent {
    UUID ID;

    IDComponent() = default;
    IDComponent(const IDComponent&) = default;
    IDComponent(UUID uuid) : ID(uuid) {}
};

struct RelationshipComponent {
    entt::entity Parent{entt::null};
    std::vector<entt::entity> Children;

    RelationshipComponent() = default;
    RelationshipComponent(const RelationshipComponent&) = default;
};

struct WorldTransformComponent {
    glm::mat4 Transform{1.0f};

    WorldTransformComponent() = default;
    WorldTransformComponent(const glm::mat4& transform) : Transform(transform) {}
    operator glm::mat4& () { return Transform; }
    operator const glm::mat4& () const { return Transform; }
};

/**
 * @brief Transform bileşeni - Her entity'nin pozisyon, rotasyon ve ölçek bilgisi
 */
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // Euler angles in radians
    glm::vec3 scale{1.0f};

    // Local matrix calculation
    glm::mat4 GetLocalMatrix() const {
        return MathUtils::CalculateTransformMatrix(position, rotation, scale);
    }
    
    // Deprecated: Use GetLocalMatrix or WorldTransformComponent
    glm::mat4 GetWorldMatrix() const { return GetLocalMatrix(); }
};

/**
 * @brief Render bileşeni - Görsellik ile ilgili bilgiler
 * 
 * Material-driven rendering architecture için tasarlanmıştır.
 * Sadece veri (handle'lar) içerir, runtime state içermez.
 */
struct RenderComponent {
    AssetHandle materialHandle;
    AssetHandle modelHandle;
    bool visible = true;
    int renderLayer = 0; // Lower values render first
    
    // Render özellikleri
    bool castsShadows = true;
    bool receivesShadows = true;
    
    RenderComponent() = default;
    
    // Constructor - Material ve model handle tabanlı
    RenderComponent(const AssetHandle& material, const AssetHandle& model)
        : materialHandle(material), modelHandle(model) {}
    
    // AssetHandle'ların geçerliliğini kontrol et
    bool HasValidHandles() const {
        return materialHandle.IsValid() && modelHandle.IsValid();
    }
    
    // Material asset handle'ını al
    AssetHandle GetMaterialHandle() const {
        return materialHandle;
    }
    
    // Model asset handle'ını al
    AssetHandle GetModelHandle() const {
        return modelHandle;
    }
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
 * @brief Light Component
 */
struct LightComponent {
    enum class LightType {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    LightType type = LightType::Point;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float range = 10.0f;           // For Point/Spot
    float innerConeAngle = 20.0f;  // For Spot (degrees)
    float outerConeAngle = 30.0f;  // For Spot (degrees)
    bool castsShadows = true;
    
    // For shader convenience
    glm::vec3 GetRadiance() const {
        return color * intensity;
    }
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


} // namespace AstralEngine
