#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace AstralEngine {

class MathUtils {
public:
    /**
     * @brief Calculates a transformation matrix from position, rotation, and scale.
     * @param position Translation vector
     * @param rotation Euler angles in radians
     * @param scale Scaling vector
     * @return Resulting transformation matrix
     */
    static glm::mat4 CalculateTransformMatrix(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale) {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        
        // Use YX Z rotation order (standard for many engines)
        glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        
        glm::mat4 scaling = glm::scale(glm::mat4(1.0f), scale);
        
        return translation * rotY * rotX * rotZ * scaling;
    }
};

} // namespace AstralEngine
