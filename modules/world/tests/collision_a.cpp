// Same-named private component types in two translation units must not share storage.
#include <astral/world/world.hpp>

#include <string>

namespace {
struct State {
    int value = 0;
};
} // namespace

astral::ComponentId state_id_a() { return astral::component_id<State>(); }
void add_state_a(astral::World& w, astral::Entity e, int v) { w.add(e, State{v}); }
int get_state_a(astral::World& w, astral::Entity e) {
    const State* s = w.get<State>(e);
    return s ? s->value : -1;
}
