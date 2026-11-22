#pragma once

#include <astral/physics/physics.hpp>

#include <span>

namespace astral {

/// Physics frame query access bound to an App's Context.
class PhysicsService {
public:
    virtual ~PhysicsService() = default;
    PhysicsService(const PhysicsService&) = delete;
    PhysicsService& operator=(const PhysicsService&) = delete;

    /// Contacts that began or ended during this frame's physics steps.
    [[nodiscard]] virtual std::span<const ContactEvent> contacts() const noexcept = 0;

protected:
    PhysicsService() = default;
};

} // namespace astral
