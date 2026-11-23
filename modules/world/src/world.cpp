#include <astral/world/world.hpp>

#include <cstring>
#include <new>

namespace astral {

namespace detail {

// --- Archetype ---------------------------------------------------------------------------

Archetype::Archetype(std::vector<const ComponentInfo*> infos) {
    std::sort(infos.begin(), infos.end(),
              [](const ComponentInfo* a, const ComponentInfo* b) { return a->id < b->id; });
    for (const ComponentInfo* info : infos) {
        ids_.push_back(info->id);
        infos_.push_back(info);
        columns_.push_back({info, nullptr});
    }
}

Archetype::~Archetype() {
    for (std::size_t row = 0; row < entities_.size(); ++row) {
        for (std::size_t c = 0; c < columns_.size(); ++c) columns_[c].info->destroy(at(c, row));
    }
    for (auto& column : columns_) {
        ::operator delete(column.data, std::align_val_t{column.info->align});
        delete[] column.added;
        delete[] column.changed;
    }
}

int Archetype::column_of(ComponentId id) const noexcept {
    const auto it = std::lower_bound(ids_.begin(), ids_.end(), id);
    return (it != ids_.end() && *it == id) ? static_cast<int>(it - ids_.begin()) : -1;
}

std::byte* Archetype::column_data(ComponentId id) noexcept {
    const int c = column_of(id);
    return c < 0 ? nullptr : columns_[static_cast<std::size_t>(c)].data;
}

void Archetype::grow() {
    const std::size_t new_capacity = capacity_ ? capacity_ * 2 : 64;
    for (auto& column : columns_) {
        const ComponentInfo& info = *column.info;
        auto* fresh = static_cast<std::byte*>(
            ::operator new(new_capacity * info.size, std::align_val_t{info.align}));
        for (std::size_t row = 0; row < entities_.size(); ++row) {
            std::byte* src = column.data + row * info.size;
            info.move_construct(fresh + row * info.size, src);
            info.destroy(src);
        }
        ::operator delete(column.data, std::align_val_t{info.align});
        column.data = fresh;
        auto* added = new std::uint32_t[new_capacity];
        auto* changed = new std::uint32_t[new_capacity];
        if (!entities_.empty()) {
            std::memcpy(added, column.added, entities_.size() * sizeof(std::uint32_t));
            std::memcpy(changed, column.changed, entities_.size() * sizeof(std::uint32_t));
        }
        delete[] column.added;
        delete[] column.changed;
        column.added = added;
        column.changed = changed;
    }
    capacity_ = new_capacity;
}

std::size_t Archetype::push(Entity entity, std::uint32_t tick) {
    if (entities_.size() == capacity_) grow();
    const std::size_t row = entities_.size();
    for (auto& column : columns_) {
        column.added[row] = tick;
        column.changed[row] = tick;
    }
    entities_.push_back(entity);
    return row;
}

Entity Archetype::remove(std::size_t row) {
    const std::size_t last = entities_.size() - 1;
    for (std::size_t c = 0; c < columns_.size(); ++c) {
        const ComponentInfo& info = *columns_[c].info;
        info.destroy(at(c, row));
        if (row != last) {
            info.move_construct(at(c, row), at(c, last));
            info.destroy(at(c, last));
            columns_[c].added[row] = columns_[c].added[last];
            columns_[c].changed[row] = columns_[c].changed[last];
        }
    }
    Entity moved{};
    if (row != last) {
        moved = entities_[last];
        entities_[row] = moved;
    }
    entities_.pop_back();
    return moved;
}

} // namespace detail

// --- Commands ----------------------------------------------------------------------------

void Commands::destroy(Entity entity) {
    push([entity](World& world) { world.destroy(entity); });
}

void Commands::apply(World& world) {
    // Commands may record more commands; keep applying until the queue is empty.
    for (;;) {
        std::vector<std::unique_ptr<Command>> batch;
        {
            std::scoped_lock lock(mutex_);
            batch.swap(commands_);
        }
        if (batch.empty()) return;
        for (auto& command : batch) command->apply(world);
    }
}

// --- World -------------------------------------------------------------------------------

namespace {

struct ThreadTickState {
    const World* world = nullptr;
    Tick baseline = 0;
    Tick tick = 1;
};

thread_local ThreadTickState tls_tick_state{};
std::atomic<WorldId> next_world_id{1};

} // namespace

World::ScopedTick::ScopedTick(const World& world, Tick baseline, Tick write_tick) noexcept
    : prev_world_(tls_tick_state.world), prev_baseline_(tls_tick_state.baseline), prev_tick_(tls_tick_state.tick) {
    tls_tick_state = {&world, baseline, write_tick};
}

World::ScopedTick::~ScopedTick() {
    tls_tick_state = {prev_world_, prev_baseline_, prev_tick_};
}

Tick World::tick() const noexcept {
    return tls_tick_state.world == this ? tls_tick_state.tick : tick_;
}

Tick World::change_baseline() const noexcept {
    return tls_tick_state.world == this ? tls_tick_state.baseline : baseline_;
}

World::World() : id_(next_world_id.fetch_add(1, std::memory_order_relaxed)) { root_ = archetype_for({}); }

World::~World() {
    archetypes_.clear(); // destroys every component value
    for (auto& [id, r] : resources_) r.destroy(r.ptr);
}

bool World::add_raw(Entity entity, const ComponentInfo& info, void* value) {
    check_not_iterating("add");
    Slot* s = slot(entity);
    if (!s) return false;
    if (std::byte* existing = get_raw(entity, info.id)) { // marks it changed
        info.move_assign(existing, value);
        return true;
    }
    detail::Archetype* to = with_component(s->archetype, info);
    const std::size_t row = move_entity(entity, *s, to);
    info.move_construct(to->column_data(info.id) + row * info.size, value);
    return true;
}

std::byte* World::get_raw(Entity entity, ComponentId id) noexcept {
    Slot* s = slot(entity);
    if (!s) return nullptr;
    std::byte* column = s->archetype->column_data(id);
    if (!column) return nullptr;
    mark_changed(*s, id);
    return column + s->row * s->archetype->column(static_cast<std::size_t>(s->archetype->column_of(id))).info->size;
}

const std::byte* World::get_raw(Entity entity, ComponentId id) const noexcept {
    const Slot* s = slot(entity);
    if (!s) return nullptr;
    const int c = s->archetype->column_of(id);
    if (c < 0) return nullptr;
    detail::Column& column = s->archetype->column(static_cast<std::size_t>(c));
    return column.data + s->row * column.info->size;
}

bool World::has_raw(Entity entity, ComponentId id) const noexcept {
    const Slot* s = slot(entity);
    return s && s->archetype->contains(id);
}

bool World::remove_raw(Entity entity, ComponentId id) {
    check_not_iterating("remove");
    Slot* s = slot(entity);
    if (!s || !s->archetype->contains(id)) return false;
    move_entity(entity, *s, without_component(s->archetype, id));
    return true;
}

void World::each_raw(std::span<const ComponentId> ids, const std::function<void(Entity)>& fn) {
    std::vector<ComponentId> sorted(ids.begin(), ids.end());
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    IterationGuard guard(*this);
    for (detail::Archetype* a : find_query(std::move(sorted))) {
        for (std::size_t row = 0; row < a->size(); ++row) fn(a->entity(row));
    }
}

void World::keep_alive(std::shared_ptr<const void> owner) {
    for (const auto& existing : keep_alive_) {
        if (existing == owner) return;
    }
    keep_alive_.push_back(std::move(owner));
}

void World::check_not_iterating(const char* operation) const {
    (void)operation;
    ASTRAL_ASSERT(iterating_ == 0,
                  "structural change (create/destroy/add/remove) inside each(): use world.commands()");
}

const World::Slot* World::slot(Entity entity) const noexcept {
    if (!entity.valid() || entity.index >= slots_.size()) return nullptr;
    const Slot& s = slots_[entity.index];
    return (s.alive && s.generation == entity.generation) ? &s : nullptr;
}

World::Slot* World::slot(Entity entity) noexcept {
    return const_cast<Slot*>(static_cast<const World*>(this)->slot(entity));
}

bool World::alive(Entity entity) const noexcept { return slot(entity) != nullptr; }

Entity World::allocate_entity() {
    std::uint32_t index;
    if (!free_slots_.empty()) {
        index = free_slots_.back();
        free_slots_.pop_back();
    } else {
        index = static_cast<std::uint32_t>(slots_.size());
        slots_.emplace_back();
    }
    Slot& s = slots_[index];
    s.alive = true;
    ++alive_count_;
    return {index, s.generation};
}

void World::place(Entity entity, detail::Archetype* archetype, std::size_t row) {
    Slot& s = slots_[entity.index];
    s.archetype = archetype;
    s.row = static_cast<std::uint32_t>(row);
}

void World::fix_moved(Entity moved, std::size_t row) {
    if (moved.valid()) slots_[moved.index].row = static_cast<std::uint32_t>(row);
}

Entity World::create() {
    check_not_iterating("create");
    Entity e = allocate_entity();
    place(e, root_, root_->push(e, tick_));
    return e;
}

EntityRef World::spawn() { return EntityRef(*this, create()); }

void World::destroy(Entity entity) {
    check_not_iterating("destroy");
    Slot* s = slot(entity);
    if (!s) return;
    fix_moved(s->archetype->remove(s->row), s->row);
    s->alive = false;
    s->archetype = nullptr;
    if (++s->generation == 0) s->generation = 1;
    free_slots_.push_back(entity.index);
    --alive_count_;
}

detail::Archetype* World::archetype_for(std::vector<const ComponentInfo*> infos) {
    std::sort(infos.begin(), infos.end(),
              [](const ComponentInfo* a, const ComponentInfo* b) { return a->id < b->id; });
    infos.erase(std::unique(infos.begin(), infos.end(),
                            [](const ComponentInfo* a, const ComponentInfo* b) { return a->id == b->id; }),
                infos.end());
    std::vector<ComponentId> ids;
    for (auto* info : infos) ids.push_back(info->id);
    if (auto it = archetype_index_.find(ids); it != archetype_index_.end()) return it->second;
    archetypes_.push_back(std::make_unique<detail::Archetype>(std::move(infos)));
    detail::Archetype* created = archetypes_.back().get();
    archetype_index_.emplace(std::move(ids), created);
    return created;
}

detail::Archetype* World::with_component(detail::Archetype* from, const ComponentInfo& info) {
    if (auto it = from->add_edges.find(info.id); it != from->add_edges.end()) return it->second;
    auto infos = from->infos();
    infos.push_back(&info);
    detail::Archetype* to = archetype_for(std::move(infos));
    from->add_edges[info.id] = to;
    to->remove_edges[info.id] = from;
    return to;
}

detail::Archetype* World::without_component(detail::Archetype* from, ComponentId id) {
    if (auto it = from->remove_edges.find(id); it != from->remove_edges.end()) return it->second;
    std::vector<const ComponentInfo*> infos;
    for (auto* info : from->infos()) {
        if (info->id != id) infos.push_back(info);
    }
    detail::Archetype* to = archetype_for(std::move(infos));
    from->remove_edges[id] = to;
    to->add_edges[id] = from;
    return to;
}

std::size_t World::move_entity(Entity entity, Slot& s, detail::Archetype* to) {
    detail::Archetype* from = s.archetype;
    const std::size_t from_row = s.row;
    const std::size_t to_row = to->push(entity, tick_);
    // Relocate shared components (keeping their change ticks); from->remove() then destroys the
    // moved-from husks and any component the destination does not have.
    for (std::size_t c = 0; c < from->column_count(); ++c) {
        const ComponentInfo& info = *from->infos()[c];
        const int dst = to->column_of(info.id);
        if (dst < 0) continue;
        const auto d = static_cast<std::size_t>(dst);
        info.move_construct(to->at(d, to_row), from->at(c, from_row));
        to->column(d).added[to_row] = from->column(c).added[from_row];
        to->column(d).changed[to_row] = from->column(c).changed[from_row];
    }
    fix_moved(from->remove(from_row), from_row);
    place(entity, to, to_row);
    return to_row;
}

std::vector<detail::Archetype*> World::find_query(std::vector<ComponentId> ids) {
    std::sort(ids.begin(), ids.end());
    std::scoped_lock lock(query_mutex_);
    auto [it, inserted] = queries_.try_emplace(ids);
    Query& q = it->second;
    const std::vector<ComponentId>& key = it->first;
    // Archetypes are only ever appended: test just the new ones.
    for (; q.checked < archetypes_.size(); ++q.checked) {
        detail::Archetype* a = archetypes_[q.checked].get();
        const bool match = std::all_of(key.begin(), key.end(),
                                       [a](ComponentId id) { return a->contains(id); });
        if (match) q.matches.push_back(a);
    }
    return q.matches;
}

void World::update_events() {
    std::scoped_lock lock(event_mutex_);
    for (auto& [id, update] : event_updaters_) {
        if (auto it = resources_.find(id); it != resources_.end()) update(it->second.ptr);
    }
}

void World::apply_commands() {
    check_not_iterating("apply_commands");
    commands_.apply(*this);
}

} // namespace astral
