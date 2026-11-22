#include "game.hpp"

#include "room.hpp"

namespace rt_showcase {

void ShowcaseGame::on_start(astral::Context& ctx) { spawn_room(ctx, view_); }

} // namespace rt_showcase
