// HelloRender: uses only the Astral::Render layer (no ECS, no engine loop).
// Shows meshes and SDF shapes in one scene, lit by a sun and two point lights.
//
//   HelloRender                       open a window
//   HelloRender --headless out.ppm    render offscreen and save an image

#include "Astral/Render/MeshPrimitives.hpp"
#include "Astral/Render/Renderer.hpp"
#include "Astral/Render/VulkanInterop.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

using namespace Astral::Render;

namespace {

void BuildScene(Renderer& renderer) {
    World& world = renderer.GetWorld();

    // --- meshes ---
    MeshHandle plane = renderer.CreateMesh(Primitives::Plane(12.0f));
    MeshHandle sphere = renderer.CreateMesh(Primitives::Sphere(0.6f));
    MeshHandle cube = renderer.CreateMesh(Primitives::Cube(0.5f));

    MaterialDesc ground;
    ground.baseColor = glm::vec4(0.55f, 0.55f, 0.52f, 1.0f);
    ground.roughness = 0.85f;
    MaterialDesc red;
    red.baseColor = glm::vec4(0.85f, 0.15f, 0.12f, 1.0f);
    red.roughness = 0.3f;
    MaterialDesc gold;
    gold.baseColor = glm::vec4(1.0f, 0.78f, 0.34f, 1.0f);
    gold.metallic = 1.0f;
    gold.roughness = 0.25f;

    world.CreateInstance(plane, renderer.CreateMaterial(ground), glm::mat4(1.0f), 1);
    world.CreateInstance(sphere, renderer.CreateMaterial(red), glm::translate(glm::mat4(1.0f), {-2.2f, 0.6f, 0.5f}), 2);
    world.CreateInstance(cube, renderer.CreateMaterial(gold),
                         glm::rotate(glm::translate(glm::mat4(1.0f), {2.3f, 0.5f, 0.2f}), 0.6f, {0, 1, 0}), 3);

    // --- SDF shapes: a smooth blob, a carved box and a torus ---
    SdfShapeDesc blob;
    blob.operation = SdfOperation::SmoothUnion;
    blob.blend = 0.35f;
    blob.material.albedo = {0.2f, 0.55f, 0.9f};
    blob.material.roughness = 0.35f;
    blob.objectId = 10;
    blob.position = {0.0f, 0.9f, 0.0f};
    blob.size = glm::vec4(0.75f);
    world.CreateSdfShape(blob);
    blob.position = {0.55f, 1.5f, 0.2f};
    blob.size = glm::vec4(0.45f);
    blob.material.albedo = {0.95f, 0.95f, 0.95f};
    world.CreateSdfShape(blob);
    blob.position = {-0.5f, 1.45f, -0.2f};
    blob.size = glm::vec4(0.4f);
    world.CreateSdfShape(blob);

    SdfShapeDesc box;
    box.primitive = SdfPrimitive::Box;
    box.position = {0.0f, 0.6f, -2.5f};
    box.size = {0.8f, 0.6f, 0.8f, 0.08f};
    box.material.albedo = {0.85f, 0.85f, 0.8f};
    box.material.roughness = 0.6f;
    box.objectId = 11;
    world.CreateSdfShape(box);
    SdfShapeDesc carve;
    carve.operation = SdfOperation::SmoothSubtract;
    carve.blend = 0.08f;
    carve.position = {0.4f, 1.1f, -2.1f};
    carve.size = glm::vec4(0.6f);
    carve.objectId = 11;
    world.CreateSdfShape(carve);

    SdfShapeDesc torus;
    torus.primitive = SdfPrimitive::Torus;
    torus.position = {0.0f, 0.25f, 2.4f};
    torus.rotation = glm::angleAxis(0.4f, glm::vec3(1, 0, 0));
    torus.size = {0.7f, 0.22f, 0.0f, 0.0f};
    torus.material.albedo = {0.9f, 0.6f, 0.2f};
    torus.material.metallic = 1.0f;
    torus.material.roughness = 0.3f;
    torus.objectId = 12;
    world.CreateSdfShape(torus);

    // --- lights ---
    LightDesc sun;
    sun.type = LightType::Directional;
    sun.direction = glm::normalize(glm::vec3(-0.7f, -0.55f, -0.45f));
    sun.color = {1.0f, 0.95f, 0.88f};
    sun.intensity = 4.0f;
    world.CreateLight(sun);

    LightDesc point;
    point.position = {-1.5f, 1.2f, 2.0f};
    point.color = {1.0f, 0.4f, 0.2f};
    point.intensity = 6.0f;
    point.range = 6.0f;
    world.CreateLight(point);
    point.position = {2.0f, 1.0f, -1.5f};
    point.color = {0.3f, 0.5f, 1.0f};
    world.CreateLight(point);
}

Camera OrbitCamera(float time) {
    Camera cam;
    float angle = 0.6f + time * 0.25f;
    cam.position = glm::vec3(std::sin(angle) * 7.0f, 3.2f, std::cos(angle) * 7.0f);
    glm::vec3 forward = glm::normalize(glm::vec3(0.0f, 0.6f, 0.0f) - cam.position);
    cam.rotation = glm::quatLookAtRH(forward, glm::vec3(0, 1, 0));
    return cam;
}

bool SavePpm(const std::string& path, const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << w << " " << h << "\n255\n";
    for (size_t i = 0; i < size_t(w) * h; ++i) out.write(reinterpret_cast<const char*>(&rgba[i * 4]), 3);
    return true;
}

int RunHeadless(const std::string& output, DebugView debug) {
    Renderer renderer;
    BuildScene(renderer);
    ViewHandle view = renderer.CreateView(960, 540);
    for (int frame = 0; frame < 24; ++frame) { // several frames so TAA converges
        if (!renderer.BeginFrame()) continue;
        ViewSettings settings;
        settings.debugView = debug;
        renderer.RenderView(view, OrbitCamera(0.0f), settings);
        renderer.EndFrame();
    }
    auto pixels = renderer.ReadViewPixels(view);
    RenderStats stats = renderer.GetStats();
    std::printf("instances=%u sdf=%u bricks=%u/%u gpu=%.2fms\n", stats.meshInstances, stats.sdfShapes, stats.sdfBricksUsed,
                stats.sdfBrickCapacity, stats.gpuFrameMs);
    return SavePpm(output, pixels, 960, 540) ? 0 : 1;
}

int RunWindowed() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Astral - HelloRender", nullptr, nullptr);

    RendererDesc desc;
    desc.applicationName = "HelloRender";
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    for (uint32_t i = 0; i < count; ++i) desc.instanceExtensions.emplace_back(extensions[i]);
    Renderer renderer(desc);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(GetVulkanHandles(renderer).instance, window, nullptr, &surface);
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    AttachSurface(renderer, surface, uint32_t(fbw), uint32_t(fbh));

    BuildScene(renderer);
    ViewHandle view = renderer.CreateView(uint32_t(fbw), uint32_t(fbh));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, 1);
        glfwGetFramebufferSize(window, &fbw, &fbh);
        if (fbw == 0 || fbh == 0) {
            glfwWaitEvents();
            continue;
        }
        if (renderer.GetViewSize(view) != glm::uvec2(fbw, fbh)) {
            renderer.ResizeView(view, uint32_t(fbw), uint32_t(fbh));
            renderer.ResizeSurface(uint32_t(fbw), uint32_t(fbh));
        }
        if (!renderer.BeginFrame()) continue;
        renderer.RenderView(view, OrbitCamera(float(glfwGetTime())));
        renderer.PresentView(view);
        renderer.EndFrame();
    }
    renderer.WaitIdle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc >= 3 && std::strcmp(argv[1], "--headless") == 0)
            return RunHeadless(argv[2], argc >= 4 ? static_cast<DebugView>(std::atoi(argv[3])) : DebugView::None);
        return RunWindowed();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return 1;
    }
}
