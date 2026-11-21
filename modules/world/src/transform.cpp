#include <astral/world/transform.hpp>

#include <unordered_map>
#include <vector>

namespace astral {

void update_transforms(World& world) {
    // Structural step first: everything with a Transform gets a WorldTransform.
    std::vector<Entity> missing;
    world.each<const Transform>([&](Entity e, const Transform&) {
        if (!world.has<WorldTransform>(e)) missing.push_back(e);
    });
    for (Entity e : missing) world.add(e, WorldTransform{});

    // Roots: world = local. Children are resolved below.
    std::vector<Entity> children;
    world.each<const Transform, WorldTransform>([&](Entity e, const Transform& t, WorldTransform& w) {
        if (world.has<Parent>(e)) {
            children.push_back(e);
        } else {
            w.matrix = t.matrix();
        }
    });

    // Each child is computed once (memoised), walking up iteratively: no recursion depth
    // limit. A parent that is dead, has no Transform, or closes a cycle ends the walk and
    // the entity at that point is treated as a root.
    enum class State : std::uint8_t { Visiting, Done };
    std::unordered_map<std::uint32_t, State> state;
    std::vector<Entity> chain;
    for (Entity start : children) {
        if (state.contains(start.index)) continue;
        chain.clear();
        Entity e = start;
        Mat4 base = Mat4::identity(); // world matrix of the first resolved ancestor
        for (;;) {
            chain.push_back(e);
            state[e.index] = State::Visiting;
            const Parent* p = world.get<Parent>(e);
            if (!p || !world.alive(p->entity) || !world.has<WorldTransform>(p->entity)) break;
            const Entity parent = p->entity;
            if (!world.has<Parent>(parent)) { // a root: already computed above
                base = world.get<WorldTransform>(parent)->matrix;
                break;
            }
            if (auto it = state.find(parent.index); it != state.end()) {
                if (it->second == State::Done) base = world.get<WorldTransform>(parent)->matrix;
                break; // Done: reuse it; Visiting: cycle, stop here
            }
            e = parent;
        }
        // Unwind from the top of the chain down to `start`.
        Mat4 current = base;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const Transform* t = world.get<Transform>(*it);
            current = current * (t ? t->matrix() : Mat4::identity());
            world.get<WorldTransform>(*it)->matrix = current;
            state[it->index] = State::Done;
        }
    }
}

} // namespace astral
