#include <astral/world/world.hpp>

#include <string>

namespace {
struct State {
    std::string text;
};
} // namespace

astral::ComponentId state_id_b() { return astral::component_id<State>(); }
void add_state_b(astral::World& w, astral::Entity e, const char* t) { w.add(e, State{t}); }
std::string get_state_b(astral::World& w, astral::Entity e) {
    const State* s = w.get<State>(e);
    return s ? s->text : "<none>";
}
