#include <astral/app/app.hpp>

#include <astral/core/clock.hpp>
#include <astral/gpu/device.hpp>
#include <astral/gpu/swapchain.hpp>
#include <astral/platform/window.hpp>
#include <astral/world/transform.hpp>

#include <array>
#include <chrono>
#include <thread>
#include <vector>

namespace astral {

struct App::Impl {
    AppDesc desc;
    Logger log;
    JobSystem jobs;
    World world;
    Input input;
    Time time;
    Clock clock;

    std::unique_ptr<Window> window;
    gpu::SurfaceSource surface;
    std::unique_ptr<gpu::Device> device;
    std::unique_ptr<gpu::Swapchain> swapchain;
    std::unique_ptr<Renderer> renderer;

    std::unique_ptr<PhysicsWorld> physics;
    float physics_accumulator = 0.0f;
    std::vector<ContactEvent> frame_contacts;
    struct BodyLink {
        Entity entity;
        BodyHandle body;
    };
    struct CharacterLink {
        Entity entity;
        CharacterHandle character;
    };
    std::vector<BodyLink> body_links;
    std::vector<CharacterLink> character_links;

    std::unique_ptr<Context> context;
    std::array<std::vector<std::unique_ptr<System>>, 4> systems;
    std::vector<DrawItem> draw_items; // scratch, reused every frame
    std::vector<std::pair<std::int32_t, SdfShape>> sdf_sorted;
    std::vector<SdfShape> sdf_shapes;
    std::vector<PointLight> point_lights;

    bool started = false;
    void sync_physics_bodies();
    void step_physics(Game& game, float delta);

    explicit Impl(const AppDesc& d) : desc(d), jobs(d.worker_threads) {}
    bool start(Game& game);
    void frame(Game& game, float delta);
};

void Context::register_system(Phase phase, std::unique_ptr<System> system) {
    app_.impl_->systems[static_cast<std::size_t>(phase)].push_back(std::move(system));
}

App::App(const AppDesc& desc) : impl_(std::make_unique<Impl>(desc)) {
    auto& m = *impl_;
    m.log.add_sink(std::make_shared<ConsoleSink>());
    m.context.reset(new Context(m.world, m.input, m.time, m.log, m.jobs, *this));
    m.physics = std::make_unique<PhysicsWorld>();
    m.context->physics = m.physics.get();
    m.time.fixed_delta = 1.0f / std::max(desc.physics_rate, 1.0f);

    if (!desc.headless) {
        m.window = Window::create({.title = desc.title, .width = desc.width, .height = desc.height});
        if (!m.window) {
            m.log.error("App: could not create a window; running headless");
        } else {
            m.surface = m.window->surface_source();
            m.device = gpu::Device::create({.app_name = desc.title, .validation = desc.validation,
                                            .surface = &m.surface, .logger = &m.log});
            if (m.device) {
                m.swapchain = gpu::Swapchain::create(*m.device, m.surface, {.vsync = desc.vsync});
                if (m.swapchain) {
                    m.renderer = std::make_unique<Renderer>(*m.device, m.swapchain->format());
                    m.context->meshes = {m.renderer->create_mesh(make_cube()),
                                         m.renderer->create_mesh(make_sphere(32, 16)),
                                         m.renderer->create_mesh(make_plane(1.0f))};
                }
            } else {
                m.log.error("App: no usable GPU; running without rendering");
            }
        }
    }
    m.context->window = m.window.get();
    m.context->device = m.device.get();
    m.context->renderer = m.renderer.get();
}

App::~App() {
    auto& m = *impl_;
    if (m.device) m.device->wait_idle();
    // Systems may release GPU resources in their destructors: they go first, while the
    // renderer and device still exist. Then the services, device last, window after it.
    for (auto& phase : m.systems) phase.clear();
    m.context->physics = nullptr;
    m.physics.reset();
    m.renderer.reset();
    m.swapchain.reset();
    m.device.reset();
}

Context& App::context() noexcept { return *impl_->context; }

std::span<const ContactEvent> Context::contacts() const noexcept { return app_.impl_->frame_contacts; }
World& App::world() noexcept { return impl_->world; }
const Time& App::time() const noexcept { return impl_->time; }

namespace {

void run_phase(std::vector<std::unique_ptr<System>>& systems, Context& ctx) {
    for (auto& s : systems) s->update(ctx);
}

/// Builds the camera from the first active Camera entity (or a default one).
CameraData find_camera(World& world, float aspect) {
    CameraData data;
    data.position = {0, 2, 6};
    data.view = look_at(data.position, {0, 0, 0}, {0, 1, 0});
    data.projection = perspective(radians(70.0f), aspect, 0.1f, 1000.0f);
    bool found = false;
    world.each<const Camera, const WorldTransform>([&](const Camera& cam, const WorldTransform& w) {
        if (found || !cam.active) return;
        found = true;
        data.position = {w.matrix[3].x, w.matrix[3].y, w.matrix[3].z};
        data.view = inverse(w.matrix);
        data.projection = perspective(cam.fov_y, aspect, cam.near_plane, cam.far_plane);
    });
    return data;
}

} // namespace

bool App::Impl::start(Game& game) {
    if (started) {
        log.error("App: run()/run_frames() may only be called once per App");
        return false;
    }
    started = true;
    game.on_start(*context);
    world.apply_commands();
    clock.tick();
    return true;
}

void App::run_frames(Game& game, std::uint32_t frames) {
    auto& m = *impl_;
    if (!m.start(game)) return;
    for (std::uint32_t i = 0; i < frames && !m.context->quit_requested(); ++i) {
        if (m.window && !m.window->poll(m.input)) break;
        const float dt = m.desc.fixed_timestep > 0 ? m.desc.fixed_timestep : m.clock.tick();
        m.frame(game, dt);
    }
    game.on_shutdown(*m.context);
    if (m.device) m.device->wait_idle();
}

int App::run(Game& game) {
    auto& m = *impl_;
    if (!m.start(game)) return 1;
    while (!m.context->quit_requested()) {
        if (m.window && !m.window->poll(m.input)) break;
        if (m.window && m.window->minimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // don't spin while hidden
            continue;
        }
        const float dt = m.desc.fixed_timestep > 0 ? m.desc.fixed_timestep : m.clock.tick();
        m.frame(game, dt);
    }
    game.on_shutdown(*m.context);
    if (m.device) m.device->wait_idle();
    return 0;
}

/// Creates bodies/characters for new components, destroys those whose entity or component is gone.
void App::Impl::sync_physics_bodies() {
    std::erase_if(body_links, [&](const BodyLink& link) {
        const RigidBody* rb = world.get<RigidBody>(link.entity);
        if (rb && rb->handle == link.body) return false;
        physics->destroy(link.body);
        return true;
    });
    std::erase_if(character_links, [&](const CharacterLink& link) {
        const CharacterController* cc = world.get<CharacterController>(link.entity);
        if (cc && cc->handle == link.character) return false;
        physics->destroy(link.character);
        return true;
    });
    world.each<RigidBody, const Transform>([&](Entity e, RigidBody& rb, const Transform& t) {
        if (physics->valid(rb.handle)) return;
        BodyDesc body_desc = rb.desc;
        body_desc.position = t.position;
        body_desc.rotation = t.rotation;
        body_desc.user_data = to_user_data(e);
        rb.handle = physics->create_body(body_desc);
        if (rb.handle.valid()) body_links.push_back({e, rb.handle});
    });
    world.each<CharacterController, const Transform>([&](Entity e, CharacterController& cc, const Transform& t) {
        if (cc.handle.valid()) return;
        CharacterDesc character_desc = cc.desc;
        character_desc.position = t.position;
        character_desc.user_data = to_user_data(e);
        cc.handle = physics->create_character(character_desc);
        if (cc.handle.valid()) character_links.push_back({e, cc.handle});
    });
}

/// Fixed-rate physics: FixedUpdate systems, kinematic targets, characters, step; then the
/// simulated poses go back into Transforms.
void App::Impl::step_physics(Game&, float delta) {
    Context& ctx = *context;
    frame_contacts.clear();
    sync_physics_bodies();
    const float fixed = time.fixed_delta;
    physics_accumulator += delta;
    std::uint32_t steps = 0;
    while (physics_accumulator >= fixed && steps < desc.max_physics_steps) {
        run_phase(systems[static_cast<std::size_t>(Phase::FixedUpdate)], ctx);
        world.apply_commands();
        sync_physics_bodies(); // FixedUpdate systems may have spawned or removed bodies
        world.each<const RigidBody, const Transform>([&](const RigidBody& rb, const Transform& t) {
            if (rb.desc.motion == MotionType::Kinematic) physics->move_kinematic(rb.handle, t.position, t.rotation, fixed);
        });
        world.each<CharacterController>([&](CharacterController& cc) {
            physics->update_character(cc.handle, cc.move, cc.jump, fixed, cc.jump_speed);
            cc.jump = false; // one jump per press
        });
        physics->step(fixed);
        frame_contacts.insert(frame_contacts.end(), physics->contacts().begin(), physics->contacts().end());
        physics_accumulator -= fixed;
        ++steps;
    }
    if (steps == desc.max_physics_steps) physics_accumulator = std::min(physics_accumulator, fixed); // drop the backlog

    world.each<const RigidBody, Transform>([&](const RigidBody& rb, Transform& t) {
        if (rb.desc.motion != MotionType::Dynamic || !physics->valid(rb.handle)) return;
        t.position = physics->position(rb.handle);
        t.rotation = physics->rotation(rb.handle);
    });
    world.each<CharacterController, Transform>([&](CharacterController& cc, Transform& t) {
        const CharacterState state = physics->character_state(cc.handle);
        if (!cc.handle.valid()) return;
        t.position = state.position;
        cc.on_ground = state.on_ground;
        cc.velocity = state.velocity;
    });
}

void App::Impl::frame(Game& game, float delta) {
    Impl& m = *this;
    Context& ctx = *m.context;
    m.time.delta = delta;

    run_phase(m.systems[static_cast<std::size_t>(Phase::PreUpdate)], ctx);
    m.world.apply_commands();
    game.on_update(ctx);
    m.world.apply_commands();
    run_phase(m.systems[static_cast<std::size_t>(Phase::Update)], ctx);
    m.world.apply_commands();
    m.step_physics(game, delta);
    m.world.apply_commands();
    run_phase(m.systems[static_cast<std::size_t>(Phase::PostUpdate)], ctx);
    m.world.apply_commands();
    update_transforms(m.world);

    if (m.renderer && m.swapchain && !(m.window && m.window->minimized())) {
        gpu::TextureHandle image = m.swapchain->acquire();
        if (image.valid()) {
            m.draw_items.clear();
            m.world.each<const MeshRenderer, const WorldTransform>(
                [&](const MeshRenderer& r, const WorldTransform& w) {
                    m.draw_items.push_back({r.mesh, w.matrix, r.color, r.roughness, r.metallic, r.emissive});
                });
            m.sdf_sorted.clear();
            m.world.each<const Sdf>([&](Entity e, const Sdf& s) {
                SdfShape shape = s.shape;
                if (const WorldTransform* w = m.world.get<WorldTransform>(e)) {
                    shape.position = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
                    shape.rotation = rotation_of(w->matrix);
                }
                m.sdf_sorted.emplace_back(s.order, shape);
            });
            std::stable_sort(m.sdf_sorted.begin(), m.sdf_sorted.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });
            m.sdf_shapes.clear();
            for (const auto& [order, shape] : m.sdf_sorted) m.sdf_shapes.push_back(shape);
            m.point_lights.clear();
            m.world.each<const PointLight>([&](Entity e, const PointLight& l) {
                PointLight light = l;
                if (const WorldTransform* w = m.world.get<WorldTransform>(e)) {
                    light.position = {w->matrix[3].x, w->matrix[3].y, w->matrix[3].z};
                }
                m.point_lights.push_back(light);
            });
            const gpu::Extent2D extent = m.swapchain->extent();
            const float aspect = extent.height ? static_cast<float>(extent.width) / static_cast<float>(extent.height) : 1.0f;
            const CameraData camera = find_camera(m.world, aspect);
            auto cmd = m.device->begin_commands();
            m.renderer->render(cmd, image, camera,
                               {.meshes = m.draw_items, .sdf = m.sdf_shapes, .lights = m.point_lights,
                                .sun = ctx.sun, .environment = ctx.environment});
            cmd.transition(image, gpu::ResourceState::Present);
            m.swapchain->present(cmd);
        }
    }

    m.time.elapsed += delta;
    ++m.time.frame;
}

} // namespace astral
