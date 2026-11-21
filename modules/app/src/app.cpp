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

    std::unique_ptr<Context> context;
    std::array<std::vector<std::unique_ptr<System>>, 3> systems;
    std::vector<DrawItem> draw_items; // scratch, reused every frame

    bool started = false;

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
    m.renderer.reset();
    m.swapchain.reset();
    m.device.reset();
}

Context& App::context() noexcept { return *impl_->context; }
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
    run_phase(m.systems[static_cast<std::size_t>(Phase::PostUpdate)], ctx);
    m.world.apply_commands();
    update_transforms(m.world);

    if (m.renderer && m.swapchain && !(m.window && m.window->minimized())) {
        gpu::TextureHandle image = m.swapchain->acquire();
        if (image.valid()) {
            m.draw_items.clear();
            m.world.each<const MeshRenderer, const WorldTransform>(
                [&](const MeshRenderer& r, const WorldTransform& w) {
                    m.draw_items.push_back({r.mesh, w.matrix, r.color});
                });
            const gpu::Extent2D extent = m.swapchain->extent();
            const float aspect = extent.height ? static_cast<float>(extent.width) / static_cast<float>(extent.height) : 1.0f;
            const CameraData camera = find_camera(m.world, aspect);
            auto cmd = m.device->begin_commands();
            m.renderer->render(cmd, image, camera, m.draw_items, ctx.light);
            cmd.transition(image, gpu::ResourceState::Present);
            m.swapchain->present(cmd);
        }
    }

    m.time.elapsed += delta;
    ++m.time.frame;
}

} // namespace astral
