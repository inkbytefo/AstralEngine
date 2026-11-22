#pragma once
// Internal to Astral::App: bridges RigidBody, CharacterController, Joint, fields and contacts to PhysicsWorld.

#include <astral/app/components.hpp>
#include <astral/physics/fields.hpp>
#include <astral/physics/physics.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace astral {

class Logger;
class Profiler;
class ScriptPhysics;
class World;

struct PhysicsBridgeConfig {
    std::uint32_t max_steps = 4;
    bool interpolation = false;
};

class PhysicsBridge {
public:
    using FixedStepCallback = void (*)(void*);

    PhysicsBridge(World& world, FieldWorld& fields, const Time& time, Profiler* profiler, Logger& log,
                  PhysicsBridgeConfig config);
    ~PhysicsBridge();
    PhysicsBridge(const PhysicsBridge&) = delete;
    PhysicsBridge& operator=(const PhysicsBridge&) = delete;

    [[nodiscard]] PhysicsWorld* world() noexcept { return physics_.get(); }
    [[nodiscard]] ScriptPhysics* script_physics() noexcept { return script_physics_.get(); }
    [[nodiscard]] std::span<const ContactEvent> contacts() const noexcept;

    void step(float delta, FixedStepCallback on_fixed_update = nullptr, void* user = nullptr);
    void shift_origin(Vec3 offset);

private:
    struct BodyLink {
        Entity entity;
        BodyHandle body;
        Vec3 previous_position{}; // kinematic bodies: pose at the start of the frame
        Quat previous_rotation{};
        Vec3 step_position{};     // physics_interpolation: simulated pose before the last step
        Quat step_rotation{};
    };
    struct CharacterLink {
        Entity entity;
        CharacterHandle character;
    };
    struct JointLink {
        Entity entity;
        JointHandle joint;
    };

    void sync_bodies();
    void update_fields();
    void move_kinematics(float alpha, float fixed);
    void update_characters(float fixed);
    void snapshot_interpolation();
    void apply_field_forces(float dt);
    void emit_contact_events();
    void write_simulated_transforms(float blend);

    World& world_;
    FieldWorld& fields_;
    const Time& time_;
    Profiler* profiler_;
    PhysicsBridgeConfig config_;
    std::unique_ptr<PhysicsWorld> physics_;
    std::unique_ptr<ScriptPhysics> script_physics_;
    float accumulator_ = 0.0f;
    std::vector<ContactEvent> frame_contacts_;
    std::vector<BodyLink> body_links_;
    std::vector<CharacterLink> character_links_;
    std::vector<JointLink> joint_links_;
    bool in_fixed_update_ = false;
};

} // namespace astral
