#pragma once

#include <glm/glm.hpp>
#include "Bounds.h"

namespace AstralEngine {

    struct Ray {
        glm::vec3 Origin;
        glm::vec3 Direction;

        Ray(const glm::vec3& origin, const glm::vec3& direction)
            : Origin(origin), Direction(direction) {}

        glm::vec3 GetPoint(float distance) const {
            return Origin + Direction * distance;
        }
    };

    inline bool RayIntersectsAABB(const Ray& ray, const AABB& aabb, float& tMin, float& tMax) {
        glm::vec3 invDir = 1.0f / ray.Direction;
        glm::vec3 t0 = (aabb.min - ray.Origin) * invDir;
        glm::vec3 t1 = (aabb.max - ray.Origin) * invDir;

        glm::vec3 tSmall = glm::min(t0, t1);
        glm::vec3 tBig = glm::max(t0, t1);

        tMin = glm::max(tSmall.x, glm::max(tSmall.y, tSmall.z));
        tMax = glm::min(tBig.x, glm::min(tBig.y, tBig.z));

        return tMax >= tMin && tMax >= 0.0f;
    }

}
