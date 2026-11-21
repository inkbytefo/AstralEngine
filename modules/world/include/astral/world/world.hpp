#pragma once

#include <astral/core/assert.hpp>
#include <astral/core/jobs.hpp>
#include <astral/world/component.hpp>
#include <astral/world/detail/archetype.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astral {

class World;
class EntityRef;

/// Structural changes recorded now and applied later with World::apply_commands().
/// Use it inside each()/parallel_each(), where the world must not change shape.
/// Recording is thread-safe (parallel_each workers may record concurrently).
class Commands {
public:
    template <class... Cs> void spawn(Cs&&... components);
    void destroy(Entity entity);
    template <class T> void add(Entity entity, T&& component);
    template <class T> void remove(Entity entity);

    [[nodiscard]] bool empty() const noexcept { return commands_.empty(); }
    void apply(World& world);

private:
    struct Command {
        virtual ~Command() = default;
        virtual void apply(World& world) = 0;
    };
    template <class Fn> struct FnCommand final : Command {
        Fn fn;
        explicit FnCommand(Fn f) : fn(std::move(f)) {}
        void apply(World& world) override { fn(world); }
    };
    template <class Fn> void push(Fn fn) {
        auto command = std::make_unique<FnCommand<Fn>>(std::move(fn));
        std::scoped_lock lock(mutex_);
        commands_.push_back(std::move(command));
    }
    std::mutex mutex_;
    std::vector<std::unique_ptr<Command>> commands_;
};

/// The ECS: entities are ids, components are plain structs, behaviour lives in systems that
/// iterate with each(). Entities with the same component set share an archetype whose
/// components are stored as tightly packed arrays (cache friendly, trivially parallel).
/// Not thread-safe, except parallel_each() which splits work internally.
class World {
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Entities ----------------------------------------------------------------------------
    Entity create();
    template <class... Cs> Entity create(Cs&&... components);
    EntityRef spawn();
    void destroy(Entity entity);
    [[nodiscard]] bool alive(Entity entity) const noexcept;
    /// Number of living entities.
    [[nodiscard]] std::size_t count() const noexcept { return alive_count_; }

    // Components --------------------------------------------------------------------------
    /// Adds or replaces a component. Returns a reference valid until the next structural change.
    template <class T> std::remove_cvref_t<T>& add(Entity entity, T&& component);
    template <class T> bool remove(Entity entity);
    template <class T> [[nodiscard]] T* get(Entity entity) noexcept;
    template <class T> [[nodiscard]] bool has(Entity entity) const noexcept;

    // Queries -----------------------------------------------------------------------------
    /// Calls fn(Cs&...) or fn(Entity, Cs&...) for every entity having all of Cs.
    /// Declare read-only access as `const T`. No structural changes inside: use commands().
    template <class... Cs, class Fn> void each(Fn&& fn);
    /// Same as each(), split across the job system. fn must only touch its own entity's data.
    template <class... Cs, class Fn>
    void parallel_each(JobSystem& jobs, Fn&& fn, std::size_t batch = 1024);
    /// Number of entities that have all of Cs.
    template <class... Cs> [[nodiscard]] std::size_t count();

    // Deferred changes ----------------------------------------------------------------------
    Commands& commands() noexcept { return commands_; }
    void apply_commands();

    // Resources: one value per type, owned by the world (game settings, time, ...) --------
    template <class T> T& set_resource(T value);
    template <class T> [[nodiscard]] T* resource() noexcept;

private:
    struct Slot {
        detail::Archetype* archetype = nullptr;
        std::uint32_t row = 0;
        std::uint32_t generation = 1;
        bool alive = false;
    };
    struct Query {
        std::vector<detail::Archetype*> matches;
        std::size_t checked = 0; // archetypes_ already tested
    };
    struct Resource {
        void* ptr = nullptr;
        void (*destroy)(void*) = nullptr;
    };
    struct IterationGuard {
        World& world;
        explicit IterationGuard(World& w) : world(w) { ++world.iterating_; }
        ~IterationGuard() { --world.iterating_; }
    };

    const Slot* slot(Entity entity) const noexcept;
    Slot* slot(Entity entity) noexcept;
    Entity allocate_entity();
    detail::Archetype* archetype_for(std::vector<const ComponentInfo*> infos);
    detail::Archetype* with_component(detail::Archetype* from, const ComponentInfo& info);
    detail::Archetype* without_component(detail::Archetype* from, ComponentId id);
    /// Moves an entity's row to `to`. Columns `to` has but `from` lacks stay uninitialised.
    std::size_t move_entity(Entity entity, Slot& s, detail::Archetype* to);
    void place(Entity entity, detail::Archetype* archetype, std::size_t row);
    void fix_moved(Entity moved, std::size_t row);
    Query& query(std::vector<ComponentId> ids);
    void check_not_iterating(const char* operation) const;

    template <class... Cs, class Fn, std::size_t... I>
    static void run_rows(detail::Archetype& a, Fn& fn, std::size_t begin, std::size_t end,
                         std::index_sequence<I...>);

    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::size_t alive_count_ = 0;
    std::vector<std::unique_ptr<detail::Archetype>> archetypes_;
    std::map<std::vector<ComponentId>, detail::Archetype*> archetype_index_;
    detail::Archetype* root_ = nullptr; // no components
    std::map<std::vector<ComponentId>, Query> queries_;
    std::unordered_map<ComponentId, Resource> resources_;
    Commands commands_;
    int iterating_ = 0;
};

/// Convenience wrapper: `world.spawn().add(Transform{}).add(Health{})`.
class EntityRef {
public:
    EntityRef(World& world, Entity entity) noexcept : world_(&world), entity_(entity) {}
    template <class T> EntityRef& add(T&& component) {
        world_->add(entity_, std::forward<T>(component));
        return *this;
    }
    template <class T> bool remove() { return world_->remove<T>(entity_); }
    template <class T> [[nodiscard]] T* get() noexcept { return world_->get<T>(entity_); }
    template <class T> [[nodiscard]] bool has() const noexcept { return world_->has<T>(entity_); }
    void destroy() { world_->destroy(entity_); }
    [[nodiscard]] bool alive() const noexcept { return world_->alive(entity_); }
    [[nodiscard]] Entity entity() const noexcept { return entity_; }
    operator Entity() const noexcept { return entity_; }

private:
    World* world_;
    Entity entity_;
};

// ===================================================================================== //
// Template implementations                                                               //
// ===================================================================================== //

namespace detail {
template <class... Ts> constexpr bool all_unique() {
    if constexpr (sizeof...(Ts) <= 1) {
        return true;
    } else {
        return []<class First, class... Rest>(std::type_identity<First>, std::type_identity<Rest>...) {
            return (!std::is_same_v<First, Rest> && ...) && all_unique<Rest...>();
        }(std::type_identity<Ts>{}...);
    }
}
} // namespace detail

template <class... Cs> Entity World::create(Cs&&... components) {
    check_not_iterating("create");
    static_assert(detail::all_unique<std::remove_cvref_t<Cs>...>(),
                  "create(): each component type may appear only once");
    if constexpr (sizeof...(Cs) == 0) {
        return create();
    } else {
        // Build the values first: an argument may point into the world's own storage,
        // which push() below can reallocate; a throwing copy must not leave a half-built row.
        std::tuple<std::remove_cvref_t<Cs>...> values(std::forward<Cs>(components)...);
        detail::Archetype* a = archetype_for({&component_info<Cs>()...});
        Entity e = allocate_entity();
        const std::size_t row = a->push(e);
        std::apply(
            [&](auto&... v) {
                ((::new (a->column_data(component_id<decltype(v)>()) + row * sizeof(v))
                      std::remove_cvref_t<decltype(v)>(std::move(v))),
                 ...);
            },
            values);
        place(e, a, row);
        return e;
    }
}

template <class T> std::remove_cvref_t<T>& World::add(Entity entity, T&& component) {
    using C = std::remove_cvref_t<T>;
    check_not_iterating("add");
    Slot* s = slot(entity);
    ASTRAL_ASSERT(s, "add() on a dead entity");
    // Copy first: `component` may live inside a column that the move below reallocates.
    C value(std::forward<T>(component));
    if (C* existing = get<C>(entity)) {
        *existing = std::move(value);
        return *existing;
    }
    detail::Archetype* to = with_component(s->archetype, component_info<C>());
    const std::size_t row = move_entity(entity, *s, to);
    return *::new (to->column_data(component_id<C>()) + row * sizeof(C)) C(std::move(value));
}

template <class T> bool World::remove(Entity entity) {
    check_not_iterating("remove");
    Slot* s = slot(entity);
    if (!s || !s->archetype->contains(component_id<T>())) return false;
    move_entity(entity, *s, without_component(s->archetype, component_id<T>()));
    return true;
}

template <class T> T* World::get(Entity entity) noexcept {
    Slot* s = slot(entity);
    if (!s) return nullptr;
    std::byte* column = s->archetype->column_data(component_id<T>());
    if (!column) return nullptr;
    return std::launder(reinterpret_cast<T*>(column + s->row * sizeof(std::remove_cvref_t<T>)));
}

template <class T> bool World::has(Entity entity) const noexcept {
    const Slot* s = slot(entity);
    return s && s->archetype->contains(component_id<T>());
}

template <class... Cs, class Fn, std::size_t... I>
void World::run_rows(detail::Archetype& a, Fn& fn, std::size_t begin, std::size_t end,
                     std::index_sequence<I...>) {
    std::array<std::byte*, sizeof...(Cs)> columns{a.column_data(component_id<Cs>())...};
    Entity* entities = a.entities();
    for (std::size_t row = begin; row < end; ++row) {
        if constexpr (std::is_invocable_v<Fn&, Entity, Cs&...>) {
            fn(entities[row], *std::launder(reinterpret_cast<Cs*>(
                                  columns[I] + row * sizeof(std::remove_cvref_t<Cs>)))...);
        } else {
            fn(*std::launder(reinterpret_cast<Cs*>(columns[I] + row * sizeof(std::remove_cvref_t<Cs>)))...);
        }
    }
}

template <class... Cs, class Fn> void World::each(Fn&& fn) {
    Query& q = query({component_id<Cs>()...});
    IterationGuard guard(*this);
    for (detail::Archetype* a : q.matches) {
        run_rows<Cs...>(*a, fn, 0, a->size(), std::index_sequence_for<Cs...>{});
    }
}

template <class... Cs, class Fn>
void World::parallel_each(JobSystem& jobs, Fn&& fn, std::size_t batch) {
    Query& q = query({component_id<Cs>()...});
    IterationGuard guard(*this);
    JobCounter counter;
    for (detail::Archetype* a : q.matches) {
        if (a->size() == 0) continue;
        jobs.parallel_for(counter, a->size(), batch, [a, &fn](std::size_t begin, std::size_t end) {
            run_rows<Cs...>(*a, fn, begin, end, std::index_sequence_for<Cs...>{});
        });
    }
    jobs.wait(counter);
}

template <class... Cs> std::size_t World::count() {
    std::size_t total = 0;
    for (detail::Archetype* a : query({component_id<Cs>()...}).matches) total += a->size();
    return total;
}

template <class T> T& World::set_resource(T value) {
    auto* fresh = new T(std::move(value)); // before touching the old one: exception-safe
    auto& r = resources_[component_id<T>()];
    if (r.ptr) r.destroy(r.ptr);
    r.ptr = fresh;
    r.destroy = [](void* p) { delete static_cast<T*>(p); };
    return *static_cast<T*>(r.ptr);
}

template <class T> T* World::resource() noexcept {
    auto it = resources_.find(component_id<T>());
    return it == resources_.end() ? nullptr : static_cast<T*>(it->second.ptr);
}

template <class... Cs> void Commands::spawn(Cs&&... components) {
    push([tuple = std::make_tuple(std::remove_cvref_t<Cs>(std::forward<Cs>(components))...)](
             World& world) mutable {
        std::apply([&](auto&&... c) { world.create(std::move(c)...); }, std::move(tuple));
    });
}

template <class T> void Commands::add(Entity entity, T&& component) {
    push([entity, c = std::remove_cvref_t<T>(std::forward<T>(component))](World& world) mutable {
        if (world.alive(entity)) world.add(entity, std::move(c));
    });
}

template <class T> void Commands::remove(Entity entity) {
    push([entity](World& world) { world.remove<T>(entity); });
}

} // namespace astral
