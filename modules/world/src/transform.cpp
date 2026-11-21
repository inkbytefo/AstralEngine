#include <astral/world/transform.hpp>

#include <vector>

namespace astral {

void update_transforms(World& world) {
    // Structural step first: everything with a Transform gets a WorldTransform.
    std::vector<Entity> missing;
    world.each<const Transform>([&](Entity e, const Transform&) {
        if (!world.has<WorldTransform>(e)) missing.push_back(e);
    });
    for (Entity e : missing) world.add(e, WorldTransform{});

    // Roots: world = local.
    std::vector<Entity> children;
    world.each<const Transform, WorldTransform>([&](Entity e, const Transform& t, WorldTransform& w) {
        if (world.has<Parent>(e)) {
            children.push_back(e);
        } else {
            w.matrix = t.matrix();
        }
    });

    // Children: walk up to the first resolved ancestor. `resolved` breaks cycles and reuse.
    std::vector<Entity> resolved_list;
    auto resolve = [&](auto&& self, Entity e, int depth) -> Mat4 {
        const Transform* t = world.get<Transform>(e);
        const Mat4 local = t ? t->matrix() : Mat4::identity();
        const Parent* p = world.get<Parent>(e);
        if (!p || !world.alive(p->entity) || !world.has<WorldTransform>(p->entity) || depth > 64) {
            return local; // root, orphan, or a cycle: treat as root
        }
        const bool parent_is_root = !world.has<Parent>(p->entity);
        const Mat4 parent = parent_is_root ? world.get<WorldTransform>(p->entity)->matrix
                                           : self(self, p->entity, depth + 1);
        return parent * local;
    };
    for (Entity e : children) world.get<WorldTransform>(e)->matrix = resolve(resolve, e, 0);
}

} // namespace astral
