#include <astral/world/transform.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace astral {

namespace {

constexpr std::size_t parallel_transform_batch = 1024;
constexpr std::uint32_t kNoChildSlot = std::numeric_limits<std::uint32_t>::max();

struct ChildNode {
    Entity entity{};
    Entity parent{};
    const Transform* local = nullptr;
    WorldTransform* world_tf = nullptr;
};

struct LevelItem {
    const Transform* local = nullptr;
    WorldTransform* world_tf = nullptr;
    const WorldTransform* parent_world_tf = nullptr;
};

void build_hierarchy_levels(World& world, const std::vector<ChildNode>& nodes,
                            std::vector<std::vector<LevelItem>>& levels) {
    std::uint32_t max_index = 0;
    for (const ChildNode& n : nodes) max_index = std::max(max_index, n.entity.index);

    std::vector<std::uint32_t> slot_of(static_cast<std::size_t>(max_index) + 1, kNoChildSlot);
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(nodes.size()); ++i) {
        slot_of[nodes[i].entity.index] = i;
    }
    auto find_child_slot = [&](Entity e) -> std::uint32_t {
        if (!e.valid() || e.index > max_index) return kNoChildSlot;
        const std::uint32_t s = slot_of[e.index];
        return (s != kNoChildSlot && nodes[s].entity == e) ? s : kNoChildSlot;
    };

    // 0 = unvisited, 1 = visiting (on current chain), 2 = done (assigned a depth level).
    std::vector<std::uint8_t> state(nodes.size(), 0);
    std::vector<std::uint32_t> depth_of(nodes.size(), 0);
    std::vector<std::uint32_t> chain;

    for (std::uint32_t start = 0; start < static_cast<std::uint32_t>(nodes.size()); ++start) {
        if (state[start] != 0) continue;
        chain.clear();
        std::uint32_t cur = start;
        std::uint32_t base_depth = 0;
        for (;;) {
            chain.push_back(cur);
            state[cur] = 1;
            const Entity parent = nodes[cur].parent;
            const std::uint32_t parent_slot = find_child_slot(parent);
            if (parent_slot == kNoChildSlot) break; // root, dead parent, or parent without Transform/Parent
            if (state[parent_slot] == 2) {
                base_depth = depth_of[parent_slot];
                break;
            }
            if (state[parent_slot] == 1) break; // cycle detected: break cycle at top of chain (depth 1)
            cur = parent_slot;
        }

        std::uint32_t current_depth = base_depth;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            const std::uint32_t idx = *it;
            ++current_depth;
            depth_of[idx] = current_depth;
            state[idx] = 2;

            const Entity parent = nodes[idx].parent;
            const std::uint32_t parent_slot = find_child_slot(parent);
            const WorldTransform* parent_wt = nullptr;
            if (parent_slot != kNoChildSlot) {
                if (depth_of[parent_slot] < current_depth) parent_wt = nodes[parent_slot].world_tf;
            } else if (world.alive(parent)) {
                parent_wt = world.get<const WorldTransform>(parent);
            }

            if (levels.size() < current_depth) levels.resize(current_depth);
            levels[current_depth - 1].push_back(LevelItem{nodes[idx].local, nodes[idx].world_tf, parent_wt});
        }
    }
}

void propagate_levels(const std::vector<std::vector<LevelItem>>& levels, JobSystem* jobs) {
    auto resolve_one = [](const LevelItem& item) {
        const Mat4 local = item.local->matrix();
        item.world_tf->matrix = item.parent_world_tf ? (item.parent_world_tf->matrix * local) : local;
    };
    for (const std::vector<LevelItem>& level : levels) {
        if (jobs && level.size() >= parallel_transform_batch) {
            JobCounter counter;
            jobs->parallel_for(counter, level.size(), parallel_transform_batch, [&](std::size_t begin, std::size_t end) {
                for (std::size_t i = begin; i < end; ++i) resolve_one(level[i]);
            });
            jobs->wait(counter);
        } else {
            for (const LevelItem& item : level) resolve_one(item);
        }
    }
}

} // namespace

void update_transforms(World& world, JobSystem* jobs) {
    // Structural step first: everything with a Transform gets a WorldTransform.
    std::vector<Entity> missing;
    world.query<const Transform>().without<WorldTransform>().each([&](Entity e, const Transform&) {
        missing.push_back(e);
    });
    for (Entity e : missing) world.add(e, WorldTransform{});

    // Roots: world = local (parallel across archetype rows when jobs is provided and root count is large).
    auto roots = world.query<const Transform, WorldTransform>().without<Parent>();
    if (jobs && roots.count() >= parallel_transform_batch) {
        roots.parallel_each(*jobs, [](const Transform& t, WorldTransform& w) { w.matrix = t.matrix(); }, parallel_transform_batch);
    } else {
        roots.each([](const Transform& t, WorldTransform& w) { w.matrix = t.matrix(); });
    }

    // Children: collect entities with Transform, WorldTransform and Parent, partition by hierarchy depth, and propagate.
    std::vector<ChildNode> children;
    world.query<const Transform, WorldTransform, const Parent>().each(
        [&](Entity e, const Transform& t, WorldTransform& w, const Parent& p) {
            children.push_back(ChildNode{e, p.entity, &t, &w});
        });
    if (!children.empty()) {
        std::vector<std::vector<LevelItem>> levels;
        build_hierarchy_levels(world, children, levels);
        propagate_levels(levels, jobs);
    }
}

} // namespace astral
