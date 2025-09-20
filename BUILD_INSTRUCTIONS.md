# Astral Engine - Modern CMake Build Instructions

## 📋 Prerequisites

### System Requirements

- **CMake**: 3.24 or newer
- **C++ Compiler**:
  - **Windows**: Visual Studio 2022 (v17.6+) or MSVC 2019+
  - **Linux**: GCC 9+ or Clang 10+
  - **macOS**: Xcode 12+ or Clang 10+
- **Git**: For dependency management

### Platform-Specific Dependencies

#### Windows
```powershell
# No additional dependencies required
# All dependencies will be automatically downloaded and built
```

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install build-essential cmake git \
    libx11-dev libxext-dev libxrandr-dev \
    libxinerama-dev libxi-dev libxss-dev \
    libgl1-mesa-dev libgles2-mesa-dev \
    libegl1-mesa-dev libdrm-dev libxkbcommon-dev \
    libwayland-dev wayland-protocols \
    libasound2-dev libpulse-dev
```

#### Linux (Fedora/RHEL)
```bash
sudo dnf install cmake gcc-c++ git \
    libX11-devel libXext-devel libXrandr-devel \
    libXinerama-devel libXi-devel libXss-devel \
    mesa-libGL-devel mesa-libGLES-devel mesa-libEGL-devel \
    libdrm-devel libxkbcommon-devel \
    wayland-devel wayland-protocols-devel \
    alsa-lib-devel pulseaudio-libs-devel
```

#### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake git
```

---

## 🚀 Build Options

### Option 1: Basic Build (Recommended)

This option automatically downloads and builds all dependencies.

#### Windows (Visual Studio)
```cmd
git clone <your-repo-url> AstralEngine
cd AstralEngine

# Create build directory
mkdir build
cd build

# Configure with default settings
cmake .. -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build . --config Release --parallel

# Run the engine
.\bin\Release\AstralEngine.exe
```

#### Linux/macOS
```bash
git clone <your-repo-url> AstralEngine
cd AstralEngine

# Create build directory
mkdir build
cd build

# Configure with default settings
cmake .. \
  -DCMAKE_BUILD_TYPE=Release

# Build the project
make -j$(nproc)  # Linux
# make -j$(sysctl -n hw.ncpu)  # macOS

# Run the engine
./bin/AstralEngine
```

### Option 2: Custom Build with Options

```bash
# Configure with custom options
cmake .. \
  -DASTRAL_BUILD_SHARED=ON \
  -DASTRAL_BUILD_EXAMPLES=ON \
  -DASTRAL_BUILD_TESTS=ON \
  -DASTRAL_BUILD_TOOLS=ON \
  -DASTRAL_USE_VULKAN=ON \
  -DASTRAL_USE_IMGUI=ON \
  -DASTRAL_USE_JOLT_PHYSICS=ON \
  -DASTRAL_ENABLE_PROFILING=OFF \
  -DASTRAL_WARNINGS_AS_ERRORS=OFF \
  -DASTRAL_ENABLE_LTO=ON \
  -DASTRAL_ENABLE_VALIDATION=ON \
  -DASTRAL_ENABLE_DEBUG_MARKERS=ON \
  -DASTRAL_ENABLE_SHADER_HOT_RELOAD=ON \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release
```

### Option 3: vcpkg Integration

```bash
# Install vcpkg if not already available
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # Linux/macOS
# .\bootstrap-vcpkg.bat  # Windows

# Install dependencies
./vcpkg install sdl3 vulkan glm nlohmann-json assimp

# Configure AstralEngine with vcpkg
cd /path/to/AstralEngine
mkdir build && cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release
```

---

## 🔧 Build Configuration Options

### Core Options

| Option | Default | Description |
|--------|---------|-------------|
| `ASTRAL_BUILD_SHARED` | `OFF` | Build AstralEngine as shared library |
| `ASTRAL_BUILD_EXAMPLES` | `ON` | Build example applications |
| `ASTRAL_BUILD_TESTS` | `OFF` | Build unit tests |
| `ASTRAL_BUILD_TOOLS` | `OFF` | Build development tools |
| `ASTRAL_ENABLE_PROFILING` | `OFF` | Enable profiling support |
| `ASTRAL_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings as compilation errors |
| `ASTRAL_ENABLE_LTO` | `ON` | Enable Link Time Optimization for Release builds |

### Subsystem Options

| Option | Default | Description |
|--------|---------|-------------|
| `ASTRAL_USE_SDL3` | `ON` | Enable SDL3 platform backend |
| `ASTRAL_USE_VULKAN` | `ON` | Enable Vulkan graphics backend |
| `ASTRAL_USE_IMGUI` | `ON` | Enable Dear ImGui UI system |
| `ASTRAL_USE_JOLT_PHYSICS` | `ON` | Enable Jolt Physics system |

### Development Options

| Option | Default | Description |
|--------|---------|-------------|
| `ASTRAL_ENABLE_VALIDATION` | `ON` | Enable Vulkan validation layers in Debug builds |
| `ASTRAL_ENABLE_DEBUG_MARKERS` | `ON` | Enable Vulkan debug markers in Debug builds |
| `ASTRAL_ENABLE_SHADER_HOT_RELOAD` | `ON` | Enable hot shader reloading |

### Advanced Configuration

```bash
# Full configuration example
cmake .. \
  -DASTRAL_BUILD_SHARED=ON \
  -DASTRAL_BUILD_EXAMPLES=ON \
  -DASTRAL_BUILD_TESTS=ON \
  -DASTRAL_BUILD_TOOLS=ON \
  -DASTRAL_USE_VULKAN=ON \
  -DASTRAL_USE_IMGUI=ON \
  -DASTRAL_USE_JOLT_PHYSICS=ON \
  -DASTRAL_ENABLE_PROFILING=ON \
  -DASTRAL_WARNINGS_AS_ERRORS=ON \
  -DASTRAL_ENABLE_LTO=ON \
  -DASTRAL_ENABLE_VALIDATION=ON \
  -DASTRAL_ENABLE_DEBUG_MARKERS=ON \
  -DASTRAL_ENABLE_SHADER_HOT_RELOAD=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="/usr/local"
```

---

## 🏃‍♂️ Running the Engine

### Windows
```cmd
# Navigate to build output
cd build\bin\Release

# Run the engine
AstralEngine.exe

# Or run from build directory
cmake --build . --target run  # If available
```

### Linux/macOS
```bash
# Navigate to build output
cd build/bin

# Run the engine
./AstralEngine

# Or run with debug output
ASTRAL_LOG_LEVEL=DEBUG ./AstralEngine
```

---

## 🔍 Verification

### Check Engine Startup

When the engine starts successfully, you should see:

```
[INFO] [Main] Starting Astral Engine...
[INFO] [Main] Engine Version: 0.2.0-alpha
[INFO] [PlatformSubsystem] Using SDL3 for platform abstraction
[INFO] [Window] SDL3 window created successfully
[INFO] [PlatformSubsystem] Window: 1280x720, VSync: enabled
[INFO] [RenderSubsystem] Vulkan renderer initialized successfully
[INFO] [AssetSubsystem] Asset manager initialized
[INFO] [ECSSubsystem] Entity Component System initialized
```

### Test Functionality

1. **Window Creation**: A window titled "Astral Engine v0.2.0-alpha" should appear
2. **Window Resizing**: Try resizing the window - should see resize events in logs
3. **Input Handling**: Press keys and move mouse - should see input events in logs
4. **Graphics Rendering**: Should see a clear colored background (indicating Vulkan is working)
5. **Close Window**: Click the X button or press Alt+F4 - engine should exit gracefully

---

## 🛠️ Troubleshooting

### Common Issues

#### 1. "CMake version too old" Error

**Solution**:
```bash
# Update CMake
# Ubuntu/Debian
sudo apt-get update && sudo apt-get install cmake

# Windows: Download from https://cmake.org/download/
# macOS
brew upgrade cmake
```

#### 2. Missing Dependencies on Linux

**Solution**:
```bash
# Install all required development packages
sudo apt-get install build-essential cmake git \
    libx11-dev libxrandr-dev libxinerama-dev \
    libxi-dev libxss-dev libgl1-mesa-dev \
    libvulkan-dev
```

#### 3. Vulkan SDK Not Found

**Solution**:
```bash
# Install Vulkan SDK
# Ubuntu/Debian
sudo apt-get install libvulkan-dev

# Download from https://vulkan.lunarg.com/ for Windows/macOS
```

#### 4. Permission Denied (Linux/macOS)

**Solution**:
```bash
# Make sure you have write permissions in build directory
sudo chown -R $USER:$USER /path/to/AstralEngine
chmod +x build/bin/AstralEngine
```

### Debug Build

For debugging issues:

```bash
# Configure debug build
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DASTRAL_ENABLE_VALIDATION=ON \
  -DASTRAL_ENABLE_DEBUG_MARKERS=ON

# Build
cmake --build . --config Debug

# Run with verbose logging
ASTRAL_LOG_LEVEL=TRACE ./bin/AstralEngine_d
```

### Verbose CMake Output

```bash
# Get detailed CMake configuration info
cmake .. --debug-find

# Or use trace mode
cmake .. --trace
```

---

## 📦 Packaging & Distribution

### Creating Release Package

```bash
# Build release version
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Install to staging directory
cmake --install . --prefix ./package

# Create archive
tar -czf AstralEngine-v0.2.0-alpha.tar.gz package/
# zip -r AstralEngine-v0.2.0-alpha.zip package/  # Windows
```

### Using CPack

```bash
# Enable CPack
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Create package
cpack
```

---

## 🔄 Continuous Integration

### GitHub Actions Example

```yaml
name: Build AstralEngine

on: [push, pull_request]

jobs:
  build:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        build_type: [Release, Debug]
        config: [
          { shared: OFF, examples: ON, tests: OFF },
          { shared: ON, examples: ON, tests: ON }
        ]
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Configure
      run: |
        cmake -B build \
          -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
          -DASTRAL_BUILD_SHARED=${{ matrix.config.shared }} \
          -DASTRAL_BUILD_EXAMPLES=${{ matrix.config.examples }} \
          -DASTRAL_BUILD_TESTS=${{ matrix.config.tests }}
    
    - name: Build
      run: cmake --build build --config ${{ matrix.build_type }}
    
    - name: Test
      if: matrix.config.tests == 'ON'
      run: ctest --test-dir build -C ${{ matrix.build_type }}
```

---

## 📞 Support

If you encounter issues:

1. **Check the logs**: Look for error messages in the console output
2. **Verify dependencies**: Ensure all required libraries are installed
3. **Check CMake cache**: Delete `build/` directory and reconfigure
4. **Review architecture document**: See `MIMARI.md` for detailed information
5. **Create an issue**: Include full error output and system information

---

## ✅ Success Indicators

Your build is working correctly if:

- ✅ CMake configuration completes without errors
- ✅ Build completes successfully
- ✅ Engine executable starts without crashing
- ✅ Window appears with correct title and size
- ✅ Keyboard and mouse input is detected
- ✅ Window can be resized and closed properly
- ✅ No Vulkan-related error messages in logs
- ✅ All selected subsystems initialize successfully

Congratulations! Your Astral Engine is now running with a modern, modular CMake configuration.