#include "physics_commands.hpp"

#include <algorithm>

namespace astral {

namespace {

std::uint64_t default_order_key(Entity e) noexcept {
    return (static_cast<std::uint64_t>(e.index) << 32) | static_cast<std::uint64_t>(e.generation);
}

} // namespace

void PhysicsCommandQueue::enqueue(PhysicsCommand cmd) {
    if (!cmd.entity.valid()) return;
    if (cmd.order_key == 0) cmd.order_key = default_order_key(cmd.entity);
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
        std::sort(out.begin(), out.end(), [](const PhysicsCommand& a, const PhysicsCommand& b) {
            return a.order_key != b.order_key ? a.order_key < b.order_key : a.sequence < b.sequence;
        });
    }
}

} // namespace astral
