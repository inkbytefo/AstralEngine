#pragma once
// Internal storage of the ECS. Public only because World's templates iterate it.

#include <astral/core/handle.hpp>
#include <astral/world/component.hpp>

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace astral {
struct EntityTag;
using Entity = Handle<EntityTag>;
} // namespace astral

namespace astral::detail {

/// One column of a component type: tightly packed values, one per entity row, plus the tick at
/// which each row's value was added and last changed (change detection).
struct Column {
    const ComponentInfo* info = nullptr;
    std::byte* data = nullptr;
    std::uint32_t* added = nullptr;
    std::uint32_t* changed = nullptr;
};

/// All entities with exactly the same set of components, stored structure-of-arrays.
class Archetype {
public:
    explicit Archetype(std::vector<const ComponentInfo*> infos);
    ~Archetype();
    Archetype(const Archetype&) = delete;
    Archetype& operator=(const Archetype&) = delete;

    [[nodiscard]] const std::vector<ComponentId>& ids() const noexcept { return ids_; }
    [[nodiscard]] const std::vector<const ComponentInfo*>& infos() const noexcept { return infos_; }
    [[nodiscard]] std::size_t size() const noexcept { return entities_.size(); }
    [[nodiscard]] Entity* entities() noexcept { return entities_.data(); }
    [[nodiscard]] Entity entity(std::size_t row) const noexcept { return entities_[row]; }

    /// Column index of `id`, or -1.
    [[nodiscard]] int column_of(ComponentId id) const noexcept;
    [[nodiscard]] bool contains(ComponentId id) const noexcept { return column_of(id) >= 0; }
    [[nodiscard]] std::byte* column_data(ComponentId id) noexcept;
    [[nodiscard]] std::size_t column_count() const noexcept { return columns_.size(); }
    [[nodiscard]] std::byte* at(std::size_t column, std::size_t row) noexcept {
        return columns_[column].data + row * columns_[column].info->size;
    }
    [[nodiscard]] Column& column(std::size_t c) noexcept { return columns_[c]; }

    /// Appends an uninitialised row for `entity` (every column stamped added/changed at `tick`);
    /// the caller constructs every column value.
    std::size_t push(Entity entity, std::uint32_t tick);
    /// Destroys the row's values and fills the hole with the last row.
    /// Returns the entity that moved into `row`, or an invalid handle if none moved.
    Entity remove(std::size_t row);

    std::unordered_map<ComponentId, Archetype*> add_edges;
    std::unordered_map<ComponentId, Archetype*> remove_edges;

private:
    void grow();

    std::vector<ComponentId> ids_;               // sorted
    std::vector<const ComponentInfo*> infos_;    // parallel to ids_
    std::vector<Column> columns_;                // parallel to ids_
    std::vector<Entity> entities_;
    std::size_t capacity_ = 0;
};

} // namespace astral::detail
