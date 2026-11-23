#pragma once

#include <astral/app/app.hpp>

#include <cstddef>

namespace material_showcase {

astral::RenderSettings showcase_settings(bool path_tracer = false);
std::size_t gallery_sphere_count();
void spawn_gallery(astral::Context& ctx);

} // namespace material_showcase
