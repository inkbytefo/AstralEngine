#pragma once

#include <astral/core/assert.hpp>
#include <astral/core/jobs.hpp>
#include <astral/world/component.hpp>
#include <astral/world/detail/archetype.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <span>
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
template <class... Cs> class QueryView;

/// Change-detection clock. Writes stamp rows with World::tick(); filters compare against the
/// world's change baseline. 32 bits: ~4 billion system runs before it wraps.
using Tick = std::uint32_t;

/// Unique runtime identity of a World instance (ADR-0034).
using WorldId = std::uint64_t;

/// Runtime entity handle paired with its owning World identity (detects cross-world misuse).
struct WorldEntityRef {
    WorldId world = 0;
    Entity entity{};
    [[nodiscard]] bool valid() const noexcept { return world != 0 && entity.valid(); }
    friend bool operator==(const WorldEntityRef&, const WorldEntityRef&) = default;
};

/// Queue of events of one type, owned by the world as a resource (World::send / EventReader).
/// Events live for two World::update_events() calls, so every system gets a chance to read them.
template <class T> class Events {
public:
    void send(T event) { current_.push_back({next_id_++, std::move(event)}); }
    /// Drops events older than one update.
    void update() {
        previous_ = std::move(current_);
        current_.clear();
    }
    /// Calls fn for events with id >= cursor (oldest first) and moves the cursor past them.
    template <class Fn> std::size_t read(std::uint64_t& cursor, Fn&& fn) const {
        std::size_t n = 0;
        for (const auto* list : {&previous_, &current_}) {
            for (const Item& item : *list) {
                if (item.id < cursor) continue;
                fn(item.value);
                ++n;
            }
        }
        cursor = next_id_;
        return n;
    }
    [[nodiscard]] std::size_t size() const noexcept { return previous_.size() + current_.size(); }

private:
    struct Item {
        std::uint64_t id;
        T value;
    };
    std::vector<Item> previous_, current_;
    std::uint64_t next_id_ = 0;
};

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
    [[nodiscard]] WorldId id() const noexcept { return id_; }
    [[nodiscard]] WorldEntityRef ref(Entity entity) const noexcept { return {id_, entity}; }
    [[nodiscard]] bool owns(WorldEntityRef handle) const noexcept { return handle.world == id_ && alive(handle.entity); }
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

    // Raw access by component id: for scripts, tools and script-defined components. Same rules as the
    // typed calls (a mutable get marks the component changed). Null / false for dead entities.
    /// Adds or replaces; `value` is moved from. False for a dead entity.
    bool add_raw(Entity entity, const ComponentInfo& info, void* value);
    [[nodiscard]] std::byte* get_raw(Entity entity, ComponentId id) noexcept;
    [[nodiscard]] const std::byte* get_raw(Entity entity, ComponentId id) const noexcept;
    [[nodiscard]] bool has_raw(Entity entity, ComponentId id) const noexcept;
    bool remove_raw(Entity entity, ComponentId id);
    /// Calls fn(Entity) for every entity that has all of `ids`. No structural changes inside.
    void each_raw(std::span<const ComponentId> ids, const std::function<void(Entity)>& fn);
    /// Keeps `owner` alive as long as the world (type data behind script-defined components).
    void keep_alive(std::shared_ptr<const void> owner);

    // Queries -----------------------------------------------------------------------------
    /// A filtered query: `world.query<Pos, const Vel>().without<Frozen>().changed<Vel>().each(fn)`.
    template <class... Cs> [[nodiscard]] QueryView<Cs...> query();
    /// Calls fn(Cs&...) or fn(Entity, Cs&...) for every entity having all of Cs.
    /// Declare read-only access as `const T`: non-const access marks the component changed.
    /// No structural changes inside: use commands().
    template <class... Cs, class Fn> void each(Fn&& fn);
    /// Same as each(), split across the job system. fn must only touch its own entity's data.
    template <class... Cs, class Fn>
    void parallel_each(JobSystem& jobs, Fn&& fn, std::size_t batch = 1024);
    /// Number of entities that have all of Cs.
    template <class... Cs> [[nodiscard]] std::size_t count();

    // Change detection -----------------------------------------------------------------------
    /// Current tick: writes are stamped with it (respects ScopedTick on worker threads).
    [[nodiscard]] Tick tick() const noexcept;
    void advance_tick() noexcept { ++tick_; }
    void set_tick(Tick tick) noexcept { tick_ = tick; }
    /// added<T>()/changed<T>() filters match rows stamped after this tick (respects ScopedTick).
    void set_change_baseline(Tick tick) noexcept { baseline_ = tick; }
    [[nodiscard]] Tick change_baseline() const noexcept;
    /// Simple mode: "changes since the previous clear_trackers()". (App instead gives every
    /// system its own baseline: changes since that system last ran.)
    void clear_trackers() noexcept {
        baseline_ = tick_;
        ++tick_;
    }

    /// Thread-local change-detection baseline and write tick for a system running on a worker thread (ADR-0032).
    class ScopedTick {
    public:
        ScopedTick(const World& world, Tick baseline, Tick write_tick) noexcept;
        ~ScopedTick();
        ScopedTick(const ScopedTick&) = delete;
        ScopedTick& operator=(const ScopedTick&) = delete;

    private:
        const World* prev_world_ = nullptr;
        Tick prev_baseline_ = 0;
        Tick prev_tick_ = 0;
    };

    // Events -------------------------------------------------------------------------------
    template <class T> void send(T event);
    /// Ages every event queue by one update (App calls it once per frame).
    void update_events();

    // Generic access (serialization, tools) -------------------------------------------------
    /// Calls fn(Entity) for every living entity. No structural changes inside.
    template <class Fn> void for_each_entity(Fn&& fn) const;

    // Deferred changes ----------------------------------------------------------------------
    Commands& commands() noexcept { return commands_; }
    void apply_commands();

    // Resources: one value per type, owned by the world (game settings, time, ...) --------
    template <class T> T& set_resource(T value);
    template <class T> [[nodiscard]] T* resource() noexcept;

private:
    template <class... Cs> friend class QueryView;
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
    std::vector<detail::Archetype*> find_query(std::vector<ComponentId> ids);
    void mark_changed(Slot& s, ComponentId id) noexcept;
    void check_not_iterating(const char* operation) const;


    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_slots_;
    std::size_t alive_count_ = 0;
    std::vector<std::unique_ptr<detail::Archetype>> archetypes_;
    std::map<std::vector<ComponentId>, detail::Archetype*> archetype_index_;
    detail::Archetype* root_ = nullptr; // no components
    std::map<std::vector<ComponentId>, Query> queries_;
    mutable std::mutex query_mutex_;
    mutable std::mutex event_mutex_;
    std::unordered_map<ComponentId, Resource> resources_;
    Commands commands_;
    std::atomic<int> iterating_{0};
    Tick tick_ = 1;
    Tick baseline_ = 0;
    std::vector<std::pair<ComponentId, void (*)(void*)>> event_updaters_;
    std::vector<std::shared_ptr<const void>> keep_alive_; // type data behind script-defined components
    WorldId id_ = 0;
};

/// Builder over World::each with filters. Cheap to create; build it where you use it.
/// with<T>: must have T (not passed to fn) · without<T>: must not have T ·
/// added<T>/changed<T>: T was added / mutably accessed after the world's change baseline.
template <class... Cs> class QueryView {
public:
    explicit QueryView(World& world) : world_(world) {}
    template <class T> QueryView& with() { with_.push_back(component_id<T>()); return *this; }
    template <class T> QueryView& without() { without_.push_back(component_id<T>()); return *this; }
    template <class T> QueryView& added() {
        with_.push_back(component_id<T>());
        added_.push_back(component_id<T>());
        return *this;
    }
    template <class T> QueryView& changed() {
        with_.push_back(component_id<T>());
        changed_.push_back(component_id<T>());
        return *this;
    }

    template <class Fn> void each(Fn&& fn);
    template <class Fn> void parallel_each(JobSystem& jobs, Fn&& fn, std::size_t batch = 1024);
    [[nodiscard]] std::size_t count();

private:
    /// Archetypes that have every component and none of the excluded ones.
    template <class Visit> void for_matches(Visit&& visit);
    template <class Fn, std::size_t... I>
    void run_rows(detail::Archetype& a, Fn& fn, std::size_t begin, std::size_t end, std::index_sequence<I...>);
    bool row_passes(detail::Archetype& a, std::size_t row) const noexcept;

    World& world_;
    std::vector<ComponentId> with_, without_, added_, changed_;
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

/// Reads events of type T sent with World::send: every reader sees every event once, as long as
/// it reads at least once per two World::update_events() calls.
template <class T> class EventReader {
public:
    /// Calls fn(const T&) for each unread event; returns how many.
    template <class Fn> std::size_t read(World& world, Fn&& fn) {
        const Events<T>* events = world.resource<Events<T>>();
        return events ? events->read(cursor_, std::forward<Fn>(fn)) : 0;
    }

private:
    std::uint64_t cursor_ = 0;
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
        const std::size_t row = a->push(e, tick_);
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
    if (C* existing = get<C>(entity)) { // get<C> (non-const) marks it changed
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
    if constexpr (!std::is_const_v<T>) mark_changed(*s, component_id<T>()); // mutable access = change
    return std::launder(reinterpret_cast<T*>(column + s->row * sizeof(std::remove_cvref_t<T>)));
}

inline void World::mark_changed(Slot& s, ComponentId id) noexcept {
    const int c = s.archetype->column_of(id);
    if (c >= 0) s.archetype->column(static_cast<std::size_t>(c)).changed[s.row] = tick();
}

template <class T> bool World::has(Entity entity) const noexcept {
    const Slot* s = slot(entity);
    return s && s->archetype->contains(component_id<T>());
}

template <class... Cs> QueryView<Cs...> World::query() { return QueryView<Cs...>(*this); }

template <class... Cs, class Fn> void World::each(Fn&& fn) { query<Cs...>().each(std::forward<Fn>(fn)); }

template <class... Cs, class Fn>
void World::parallel_each(JobSystem& jobs, Fn&& fn, std::size_t batch) {
    query<Cs...>().parallel_each(jobs, std::forward<Fn>(fn), batch);
}

template <class... Cs> std::size_t World::count() { return query<Cs...>().count(); }

template <class Fn> void World::for_each_entity(Fn&& fn) const {
    for (std::uint32_t i = 0; i < slots_.size(); ++i) {
        if (slots_[i].alive) fn(Entity{i, slots_[i].generation});
    }
}

template <class T> void World::send(T event) {
    std::scoped_lock lock(event_mutex_);
    auto* events = resource<Events<T>>();
    if (!events) {
        events = &set_resource(Events<T>{});
        event_updaters_.emplace_back(component_id<Events<T>>(), [](void* p) { static_cast<Events<T>*>(p)->update(); });
    }
    events->send(std::move(event));
}

// --- QueryView ---------------------------------------------------------------------------------

template <class... Cs> template <class Visit> void QueryView<Cs...>::for_matches(Visit&& visit) {
    std::vector<ComponentId> ids{component_id<Cs>()...};
    ids.insert(ids.end(), with_.begin(), with_.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    for (detail::Archetype* a : world_.find_query(std::move(ids))) {
        if (a->size() == 0) continue;
        const bool excluded = std::any_of(without_.begin(), without_.end(), [a](ComponentId id) { return a->contains(id); });
        if (!excluded) visit(*a);
    }
}

template <class... Cs>
bool QueryView<Cs...>::row_passes(detail::Archetype& a, std::size_t row) const noexcept {
    const Tick since = world_.change_baseline();
    for (ComponentId id : added_) {
        if (a.column(static_cast<std::size_t>(a.column_of(id))).added[row] <= since) return false;
    }
    for (ComponentId id : changed_) {
        if (a.column(static_cast<std::size_t>(a.column_of(id))).changed[row] <= since) return false;
    }
    return true;
}

template <class... Cs>
template <class Fn, std::size_t... I>
void QueryView<Cs...>::run_rows(detail::Archetype& a, Fn& fn, std::size_t begin, std::size_t end,
                                std::index_sequence<I...>) {
    std::array<std::byte*, sizeof...(Cs)> columns{a.column_data(component_id<Cs>())...};
    // Change ticks of the components accessed mutably (nullptr for const ones).
    std::array<std::uint32_t*, sizeof...(Cs)> stamps{
        (std::is_const_v<Cs> ? nullptr : a.column(static_cast<std::size_t>(a.column_of(component_id<Cs>()))).changed)...};
    const bool filtered = !added_.empty() || !changed_.empty();
    const Tick now = world_.tick();
    Entity* entities = a.entities();
    for (std::size_t row = begin; row < end; ++row) {
        if (filtered && !row_passes(a, row)) continue;
        ((stamps[I] ? (void)(stamps[I][row] = now) : (void)0), ...);
        if constexpr (std::is_invocable_v<Fn&, Entity, Cs&...>) {
            fn(entities[row], *std::launder(reinterpret_cast<Cs*>(columns[I] + row * sizeof(std::remove_cvref_t<Cs>)))...);
        } else {
            fn(*std::launder(reinterpret_cast<Cs*>(columns[I] + row * sizeof(std::remove_cvref_t<Cs>)))...);
        }
    }
}

template <class... Cs> template <class Fn> void QueryView<Cs...>::each(Fn&& fn) {
    World::IterationGuard guard(world_);
    for_matches([&](detail::Archetype& a) { run_rows(a, fn, 0, a.size(), std::index_sequence_for<Cs...>{}); });
}

template <class... Cs>
template <class Fn>
void QueryView<Cs...>::parallel_each(JobSystem& jobs, Fn&& fn, std::size_t batch) {
    World::IterationGuard guard(world_);
    JobCounter counter;
    for_matches([&](detail::Archetype& a) {
        jobs.parallel_for(counter, a.size(), batch, [this, &a, &fn](std::size_t begin, std::size_t end) {
            run_rows(a, fn, begin, end, std::index_sequence_for<Cs...>{});
        });
    });
    jobs.wait(counter);
}

template <class... Cs> std::size_t QueryView<Cs...>::count() {
    std::size_t total = 0;
    const bool filtered = !added_.empty() || !changed_.empty();
    for_matches([&](detail::Archetype& a) {
        if (!filtered) {
            total += a.size();
            return;
        }
        for (std::size_t row = 0; row < a.size(); ++row) total += row_passes(a, row) ? 1 : 0;
    });
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
