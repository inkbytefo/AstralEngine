#pragma once

#include <astral/world/scene.hpp>
#include <astral/world/transform.hpp>

#include <string_view>

namespace astral {

/// Scene and prefab I/O bound to an App's Context.
class SceneService {
public:
    virtual ~SceneService() = default;
    SceneService(const SceneService&) = delete;
    SceneService& operator=(const SceneService&) = delete;

    /// Appends a scene file (relative to the asset root) to the world. Its "sun" and "environment",
    /// if present, replace ctx.sun / ctx.environment. Problems are logged and returned.
    virtual LoadResult load_scene(std::string_view path) = 0;
    /// Writes the world as a scene file (with ctx.sun / ctx.environment). False if it cannot be written.
    virtual bool save_scene(std::string_view path) = 0;
    /// Creates an instance of a prefab now: its parts exist when this returns. Components of the
    /// prefab's root other than Transform come from the prefab.
    virtual Entity spawn_prefab(std::string_view path, const Transform& transform = {}) = 0;

protected:
    SceneService() = default;
};

} // namespace astral
