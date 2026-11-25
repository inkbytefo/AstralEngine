#pragma once

#include <astral/world/world.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace astral {

class Context;

/// When a system runs inside a frame:
/// input -> PreUpdate -> Game::on_update -> Update -> physics (FixedUpdate systems + step, 0..n
/// times at a fixed rate) -> PostUpdate -> transforms -> render. Commands are applied after each.
enum class Phase : std::uint8_t { PreUpdate, Update, FixedUpdate, PostUpdate };

/// Declares which components a System reads and writes so non-conflicting systems can run in parallel (ADR-0032).
/// A system that declares nothing (or calls mark_exclusive()) runs alone on the main thread.
class SystemAccess {
public:
    template <class... Cs> SystemAccess& reads() {
        (add_read(component_id<Cs>()), ...);
        return *this;
    }
    template <class... Cs> SystemAccess& writes() {
        (add_write(component_id<Cs>()), ...);
        return *this;
    }
    /// Infers reads (const T) and writes (non-const T) from a query signature: `access.query<const Vel, Pos>()`.
    template <class... Cs> SystemAccess& query() {
        ((std::is_const_v<Cs> ? add_read(component_id<Cs>()) : add_write(component_id<Cs>())), ...);
        return *this;
    }
    SystemAccess& mark_exclusive() noexcept {
        exclusive_ = true;
        return *this;
    }

    void add_read(ComponentId id);
    void add_write(ComponentId id);
    [[nodiscard]] bool exclusive() const noexcept { return exclusive_ || (reads_.empty() && writes_.empty()); }
    [[nodiscard]] std::span<const ComponentId> read_ids() const noexcept { return reads_; }
    [[nodiscard]] std::span<const ComponentId> write_ids() const noexcept { return writes_; }
    [[nodiscard]] bool conflicts_with(const SystemAccess& other) const noexcept;

private:
    std::vector<ComponentId> reads_;
    std::vector<ComponentId> writes_;
    bool exclusive_ = false;
};

/// Game logic that runs every frame. Read and write components through ctx.world.
/// Inside update(), `added<T>()`/`changed<T>()` query filters mean "since this system last ran".
/// Declare component access with `reads<...>()` / `writes<...>()` (or override `configure`) to allow
/// parallel execution alongside non-conflicting systems in the same phase (ADR-0032).
class System {
public:
    virtual ~System() = default;
    virtual void configure(SystemAccess&) {}
    virtual void update(Context& ctx) = 0;
    /// Its type name ("PlayerControl"): how the profiler and debug UI show it.
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const SystemAccess& access() const noexcept { return access_; }

protected:
    template <class... Cs> void reads() { access_.reads<Cs...>(); }
    template <class... Cs> void writes() { access_.writes<Cs...>(); }
    template <class... Cs> void query_access() { access_.query<Cs...>(); }
    void mark_exclusive() noexcept { access_.mark_exclusive(); }

private:
    friend class App;
    friend class Context;
    friend class SystemScheduler;
    Tick last_run_ = 0;
    std::string name_;
    SystemAccess access_;
};

/// Lightweight callback-backed System created by `Context::add_system(phase, name, fn, access)`.
class FunctionSystem final : public System {
public:
    explicit FunctionSystem(std::function<void(Context&)> fn, SystemAccess access = {})
        : fn_(std::move(fn)), initial_access_(std::move(access)) {}

    void configure(SystemAccess& out) override { out = initial_access_; }
    void update(Context& ctx) override {
        if (fn_) fn_(ctx);
    }

private:
    std::function<void(Context&)> fn_;
    SystemAccess initial_access_;
};

} // namespace astral
