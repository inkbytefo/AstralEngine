#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <any>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace AstralEngine {

// Performance veri tipleri
struct PerformanceData {
    float cpuTime;
    float gpuTime;
    uint32_t drawCalls;
    uint32_t triangles;
    size_t memoryUsage;
    std::chrono::system_clock::time_point timestamp;
    
    PerformanceData()
        : cpuTime(0.0f), gpuTime(0.0f), drawCalls(0), triangles(0), memoryUsage(0),
          timestamp(std::chrono::system_clock::now()) {}
};

// Entity veri tipleri
struct EntitySelectionData {
    uint32_t entityId;
    std::string entityName;
    std::vector<std::string> componentTypes;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
    
    EntitySelectionData()
        : entityId(0), position(0.0f), rotation(1.0f, 0.0f, 0.0f, 0.0f), scale(1.0f) {}
};

// Material veri tipleri
struct MaterialData {
    std::string name;
    std::string shaderPath;
    std::unordered_map<std::string, std::any> properties;
    std::unordered_map<std::string, std::string> textures;
    bool isDirty;
    
    MaterialData() : isDirty(false) {}
};

// Debug veri tipleri
struct DebugDrawData {
    enum class Type { Line, Box, Sphere, Text, Frustum };
    
    Type type;
    glm::vec3 start;
    glm::vec3 end;
    glm::vec3 center;
    glm::vec3 size;
    float radius;
    glm::vec4 color;
    std::string text;
    float duration;
    bool depthTest;
    
    DebugDrawData()
        : type(Type::Line), start(0.0f), end(0.0f), center(0.0f), size(1.0f),
          radius(1.0f), color(1.0f, 1.0f, 1.0f, 1.0f), duration(0.0f), depthTest(true) {}
};

// Console veri tipleri
struct ConsoleCommandData {
    std::string name;
    std::string description;
    std::vector<std::string> parameters;
    std::function<void(const std::vector<std::string>&)> execute;
    
    ConsoleCommandData() = default;
};

struct ConsoleVariableData {
    std::string name;
    std::string description;
    std::any value;
    std::type_index type;
    bool readOnly;
    
    ConsoleVariableData() : type(typeid(void)), readOnly(false) {}
};

} // namespace AstralEngine