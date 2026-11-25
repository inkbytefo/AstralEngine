#pragma once
// Internal to Astral::App: WindZone / WaterBody field rebuilding and aerodynamic/buoyancy forces (ADR-0029).

namespace astral {

class FieldWorld;
class PhysicsWorld;
class World;

/// Rebuilds `fields` from WindZone and WaterBody entities in `world`.
void rebuild_field_world(World& world, FieldWorld& fields);

/// Applies Buoyant, AirDrag, and Airfoil forces onto dynamic RigidBodies.
void apply_physics_field_forces(World& world, PhysicsWorld& physics, const FieldWorld& fields, double now, float dt);

} // namespace astral
