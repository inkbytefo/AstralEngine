#pragma once

#include "Astral/Core/EntityHandle.hpp"
#include <glm/glm.hpp>

namespace Astral {
/// Frame-local camera snapshot. Does not own or reference ECS component storage.
struct RenderCamera {
    EntityHandle entity = NullEntityHandle;
    uint64_t sceneInstance = 0;
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float nearClip = 0.01f;
    float farClip = 50.0f;
};
}
