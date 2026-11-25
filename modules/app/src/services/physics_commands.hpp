#pragma once
// Internal to Astral::App: thread-safe deterministic physics command queue (ADR-0034, ADR-0038).

#include <astral/core/math.hpp>
#include <astral/physics/physics.hpp>
#include <astral/world/world.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace astral {

enum class PhysicsCommandKind : std::uint8_t {
    Impulse,
    Force,
    SetLinearVelocity,
    SetAngularVelocity,
    SetBodyTransform,
    WakeBody,
    SleepBody,
    SetJointMotorMode,
    SetJointMotorVelocity,
    SetJointMotorPosition,
    SetJointMotorAngularVelocity,
    SetJointMotorRotation,
    BreakJoint,
};

struct PhysicsCommand {
    std::uint64_t order_key = 0;
    std::uint64_t sequence = 0;
    Entity entity{};
    PhysicsCommandKind kind = PhysicsCommandKind::Impulse;
    Vec3 value{};
    Quat quat{};
    float scalar = 0.0f;
    JointMotorMode motor_mode = JointMotorMode::Off;
};

/// Collects physics commands from parallel systems and scripts and drains them in deterministic order.
class PhysicsCommandQueue {
public:
    void enqueue(PhysicsCommand cmd);
    [[nodiscard]] std::size_t pending_count() const noexcept;
    void drain_sorted(std::vector<PhysicsCommand>& out);

private:
    mutable std::mutex mutex_;
    std::vector<PhysicsCommand> pending_;
    std::uint64_t next_seq_ = 0;
};

} // namespace astral
