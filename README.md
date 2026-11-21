# AstralEngine

A modular C++20 game engine **library**. Your game owns the main loop and calls
the engine; the engine never calls your game.

```cpp
astral::Logger log;
astral::JobSystem jobs;
astral::Clock clock;

while (running) {
    float dt = clock.tick();
    game.update(dt);          // your code
}
```

## Status

| Module | Status |
|---|---|
| `Astral::Core` — log, handles, clock, jobs | ✅ M0 |
| `Astral::Gpu` (Vulkan 1.3 RHI), `Astral::Platform` (SDL3) | ✅ M1 |
| `Astral::World` (own ECS), `Astral::App` | M2 |
| `Astral::Render` (render graph, deferred + SDF) | M3 |
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
find_package(Astral 0.2 CONFIG REQUIRED COMPONENTS Core)
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
