#pragma once
// The "RGB room": red / green / blue walls, a mirror-chrome sphere on a plinth, glossy and rough boxes, a gold SDF
// torus, rubbery SDF blobs and a single ceiling panel light.

#include <astral/astral.hpp>

namespace rt_showcase {

/// Where the camera stands: --view 0..2.
struct View {
    astral::Vec3 eye, target;
    float fov;
};
[[nodiscard]] const View& view(int index);

/// Renderer settings that suit the room (the GI volume covers it).
[[nodiscard]] astral::RenderSettings showcase_settings();
[[nodiscard]] astral::Environment showcase_environment();

/// Creates the room, the light and the camera for `view_index`.
void spawn_room(astral::Context& ctx, int view_index);

/// Number of mesh objects spawn_room creates.
[[nodiscard]] std::size_t room_object_count();

} // namespace rt_showcase
