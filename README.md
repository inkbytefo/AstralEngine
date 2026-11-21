# AstralEngine

A modular C++20 game engine built as a set of libraries, with an App layer on top for writing games.

```cpp
#include <astral/astral.hpp>
using namespace astral;

struct Spin { float speed = 1.0f; };

class SpinSystem final : public System {
    void update(Context& ctx) override {
        ctx.world.each<Transform, const Spin>([&](Transform& t, const Spin& s) {
            t.rotation = Quat::from_axis_angle({0, 1, 0}, s.speed * ctx.time.delta) * t.rotation;
        });
    }
};

class MyGame final : public Game {
    void on_start(Context& ctx) override {
        ctx.world.create(Transform{}, MeshRenderer{ctx.meshes.cube}, Spin{2.0f});
        ctx.add_system<SpinSystem>(Phase::Update);
    }
};

int main() { return App({.title = "My Game"}).run<MyGame>(); }
```

Games are written with components (data) and systems (logic) on top of `Astral::App`; every lower
module (core, gpu, platform, world, render) is also usable directly as a plain library.

## Status

| Module | Status |
|---|---|
| `Astral::Core` — log, handles, clock, jobs | ✅ M0 |
| `Astral::Gpu` (Vulkan 1.3 RHI), `Astral::Platform` (SDL3) | ✅ M1 |
| `Astral::World` (own ECS), `Astral::Render` (forward), `Astral::App` | ✅ M2 |
| Render graph, deferred + SDF | M3 |
| `Astral::Physics` (Jolt) | M4 |
| `Astral::Assets` (glTF) | M5 |

## Build

Requires CMake ≥ 3.24, Ninja, a C++20 compiler (GCC 13, Clang 18, MSVC 2022) and the Vulkan SDK
(headers, loader, `glslc`). SDL3 and VMA are fetched automatically.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Presets: `debug`, `release`, `tsan` (ThreadSanitizer).

## Use in your game

```sh
cmake --install build/release --prefix <install-dir>
```

```cmake
find_package(Astral 0.3 CONFIG REQUIRED COMPONENTS Core)
target_link_libraries(MyGame PRIVATE Astral::Core)
```

Or add the repo with `add_subdirectory` / `FetchContent` and link the same targets.

## Layout

```
modules/<name>/include/astral/<name>/   public API
modules/<name>/src/                     private implementation
modules/<name>/tests/                   unit tests
examples/                               runnable samples
tests/consumer/                         find_package smoke test
cmake/                                  build helpers
```

## Rules

- Dependencies point one way: `core → platform/gpu → render/world/physics/assets → app`.
- No singletons, no global mutable state — pass dependencies in.
- No third-party types in public headers.
- Resources are referenced by generational `Handle<T>`, not raw pointers.

## Docs

See [docs/](docs/README.md) (Turkish): architecture, roadmap, module docs, decision records.

## License

MIT
