# Building and Running

## Prerequisites

Ensure you have the following installed on your system:

1.  **C++ Compiler:** MSVC (Visual Studio 2022 recommended) with C++20 support.
2.  **CMake:** Version 3.20 or higher.
3.  **Vulkan SDK:** Latest version (1.3+ required). Ensure `VULKAN_SDK` environment variable is set.
4.  **Git:** For cloning the repository.

## Dependencies

The project uses CMake's `FetchContent` to manage dependencies automatically:
- **SDL3:** For windowing and input.
- **GLM:** For mathematics.
- **ImGui:** For user interface (Docking branch).
- **stb_image:** For image loading.
- **Vulkan Memory Allocator (VMA):** Included in source.

## Build Instructions

### Windows (PowerShell)

```powershell
# 1. Create a build directory
mkdir Build
cd Build

# 2. Configure the project
cmake ..

# 3. Build (Debug configuration)
cmake --build . --config Debug

# 4. Build (Release configuration)
cmake --build . --config Release
```

## Running the Examples

After a successful build, the executables are located in the `Build/Examples/Debug` (or Release) folder.

### Render Test
A basic verification of the rendering pipeline (spinning cube/triangle).

```powershell
.\Examples\Debug\RenderTest_d.exe
```

*Note: Ensure you run the executable from the project root or build root so that relative asset paths (`Assets/`) are resolved correctly.*
