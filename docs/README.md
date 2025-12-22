# Astral Engine

**Astral Engine** is a high-performance, modular, and autonomous "AI Operating System" framework designed for scalability and professional-grade rendering. It is built with modern C++20 and leverages Vulkan 1.3 for state-of-the-art graphics.

## Project Vision

The goal of Astral Engine is to provide a robust foundation for AI-driven applications and high-fidelity simulations. It prioritizes:
- **Modularity:** Plug-and-play subsystem architecture.
- **Performance:** Low-overhead RHI (Render Hardware Interface) with a Vulkan backend.
- **Modernity:** Usage of C++20 standards, Dynamic Rendering, and bindless descriptors (planned).
- **Stability:** Strict typing, RAII resource management, and comprehensive error handling.

## Documentation Index

- [**Architecture**](ARCHITECTURE.md): High-level system design, subsystems, and data flow.
- [**Renderer**](RENDERER.md): Deep dive into the RHI, Vulkan backend, and rendering pipeline.
- [**Building & Running**](BUILDING.md): Instructions for setting up the environment and compiling the engine.
- [**Coding Standards**](CODING_STANDARDS.md): Guidelines for contributing to the codebase.

## Quick Start

```bash
# Clone the repository
git clone https://github.com/your-repo/AstralEngine.git
cd AstralEngine

# Build
mkdir Build
cd Build
cmake ..
cmake --build . --config Debug

# Run Examples
.\Examples\Debug\RenderTest_d.exe
```

## License

[MIT License](../LICENSE) (or appropriate license)
