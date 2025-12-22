#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <limits>

namespace AstralEngine {

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    AABB() : min(std::numeric_limits<float>::max()), max(std::numeric_limits<float>::lowest()) {}
    AABB(const glm::vec3& min, const glm::vec3& max) : min(min), max(max) {}

    bool IsValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    glm::vec3 GetCenter() const {
        return (min + max) * 0.5f;
    }

    glm::vec3 GetExtent() const {
        return max - min;
    }

    void Merge(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void Merge(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    void Extend(const glm::vec3& point) {
        Merge(point);
    }

    void Extend(const AABB& other) {
        Merge(other);
    }
};

} // namespace AstralEngine
