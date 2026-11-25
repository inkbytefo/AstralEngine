#include "physics_commands.hpp"

#include "command_producer.hpp"

#include <algorithm>
#include <tuple>

namespace astral {

namespace detail {

namespace {
thread_local std::uint64_t t_command_producer = 0;
} // namespace

std::uint64_t current_command_producer() noexcept { return t_command_producer; }

ScopedCommandProducer::ScopedCommandProducer(std::uint64_t rank) noexcept : previous_(t_command_producer) {
    t_command_producer = rank;
}
ScopedCommandProducer::~ScopedCommandProducer() { t_command_producer = previous_; }

} // namespace detail

namespace {

std::uint64_t default_order_key(Entity e) noexcept {
    return (static_cast<std::uint64_t>(e.index) << 32) | static_cast<std::uint64_t>(e.generation);
}

} // namespace

void PhysicsCommandQueue::enqueue(PhysicsCommand cmd) {
    if (!cmd.entity.valid()) return;
    if (cmd.order_key == 0) cmd.order_key = default_order_key(cmd.entity);
    cmd.producer = detail::current_command_producer();
    std::scoped_lock lock(mutex_);
    cmd.sequence = next_seq_++;
    pending_.push_back(cmd);
}

std::size_t PhysicsCommandQueue::pending_count() const noexcept {
    std::scoped_lock lock(mutex_);
    return pending_.size();
}

void PhysicsCommandQueue::drain_sorted(std::vector<PhysicsCommand>& out) {
    out.clear();
    {
        std::scoped_lock lock(mutex_);
        out.swap(pending_);
    }
    if (out.size() > 1) {
        // Producer (registration order) before sequence: the sequence comes from lock order, which
        // only orders commands of one producer (one thread) deterministically (ADR-0043).
        std::sort(out.begin(), out.end(), [](const PhysicsCommand& a, const PhysicsCommand& b) {
            return std::tie(a.order_key, a.producer, a.sequence) < std::tie(b.order_key, b.producer, b.sequence);
        });
    }
}

void PhysicsCommandQueue::shift_positions(Vec3 offset) {
    std::scoped_lock lock(mutex_);
    for (PhysicsCommand& cmd : pending_) {
        if (cmd.kind == PhysicsCommandKind::SetBodyTransform) cmd.value = cmd.value - offset;
    }
}

} // namespace astral
