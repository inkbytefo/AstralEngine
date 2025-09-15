# Astral Engine - SDL3 Integration Build Instructions

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
# SDL3 will be automatically downloaded and built
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

### Option 1: Automatic SDL3 Build (Recommended)

This option automatically downloads and builds SDL3 from source.

#### Windows (Visual Studio)
```cmd
git clone <your-repo-url> AstralEngine
cd AstralEngine

# Create build directory
mkdir build
cd build

# Configure with automatic SDL3 build
cmake .. -G "Visual Studio 17 2022" -A x64 \
  -DASTRAL_VENDOR_SDL3=ON \
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

# Configure with automatic SDL3 build
cmake .. \
  -DASTRAL_VENDOR_SDL3=ON \
  -DCMAKE_BUILD_TYPE=Release

# Build the project
make -j$(nproc)  # Linux
# make -j$(sysctl -n hw.ncpu)  # macOS

# Run the engine
./bin/AstralEngine
```

### Option 2: Pre-installed SDL3

If you have SDL3 already installed on your system.

#### Download SDL3 Development Libraries

1. **Visit**: https://github.com/libsdl-org/SDL/releases
2. **Download**: SDL3-devel-3.2.22-VC.zip (Windows) or equivalent for your platform
3. **Extract**: To a known location (e.g., `C:\SDL3` on Windows)

#### Build with Pre-installed SDL3

```bash
# Configure with SDL3 path
cmake .. \
  -DSDL3_ROOT="/path/to/your/sdl3/installation" \
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

# Install SDL3
./vcpkg install sdl3

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
| `ASTRAL_VENDOR_SDL3` | `OFF` | Automatically download and build SDL3 |
| `ASTRAL_LINK_SDL3MAIN` | `ON` | Link SDL3main on Windows |
| `ASTRAL_BUILD_EXAMPLES` | `ON` | Build example applications |
| `ASTRAL_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings as compilation errors |
| `ASTRAL_ENABLE_PROFILING` | `OFF` | Enable profiling support |
| `ASTRAL_ENABLE_TESTS` | `OFF` | Build unit tests |

### Advanced Configuration

```bash
# Full configuration example
cmake .. \
  -DASTRAL_VENDOR_SDL3=ON \
  -DASTRAL_BUILD_EXAMPLES=ON \
  -DASTRAL_WARNINGS_AS_ERRORS=ON \
  -DASTRAL_ENABLE_PROFILING=ON \
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

### Check SDL3 Integration

When the engine starts successfully, you should see:

```
[INFO] [Main] Starting Astral Engine...
[INFO] [Main] Engine Version: 0.1.0-alpha
[INFO] [PlatformSubsystem] Using SDL3 for platform abstraction
[INFO] [Window] SDL3 window created successfully
[INFO] [PlatformSubsystem] Window: 1280x720, VSync: enabled
```

### Test Window Functionality

1. **Window Creation**: A window titled "Astral Engine v0.1.0-alpha" should appear
2. **Window Resizing**: Try resizing the window - should see resize events in logs
3. **Input Handling**: Press keys and move mouse - should see input events in logs
4. **Close Window**: Click the X button or press Alt+F4 - engine should exit gracefully

---

## 🛠️ Troubleshooting

### Common Issues

#### 1. "SDL3 not found" Error

**Solution**:
```bash
# Enable automatic build
cmake .. -DASTRAL_VENDOR_SDL3=ON

# Or specify SDL3 path manually
cmake .. -DSDL3_ROOT="/path/to/sdl3"
```

#### 2. "CMake version too old" Error

**Solution**:
```bash
# Update CMake
# Ubuntu/Debian
sudo apt-get update && sudo apt-get install cmake

# Windows: Download from https://cmake.org/download/
# macOS
brew upgrade cmake
```

#### 3. Missing DLL on Windows

**Solution**:
```cmd
# SDL3.dll should be automatically copied to output directory
# If not, manually copy from SDL3 installation:
copy "C:\SDL3\bin\x64\SDL3.dll" "build\bin\Release\"
```

#### 4. Linux Build Dependencies Missing

**Solution**:
```bash
# Install all required development packages
sudo apt-get install build-essential cmake git \
    libx11-dev libxrandr-dev libxinerama-dev \
    libxi-dev libxss-dev libgl1-mesa-dev
```

#### 5. Permission Denied (Linux/macOS)

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
  -DASTRAL_VENDOR_SDL3=ON

# Build
cmake --build . --config Debug

# Run with verbose logging
ASTRAL_LOG_LEVEL=TRACE ./bin/AstralEngine_Debug
```

### Verbose CMake Output

```bash
# Get detailed CMake configuration info
cmake .. --debug-find -DASTRAL_VENDOR_SDL3=ON

# Or use trace mode
cmake .. --trace -DASTRAL_VENDOR_SDL3=ON
```

---

## 📦 Packaging & Distribution

### Creating Release Package

```bash
# Build release version
cmake .. -DCMAKE_BUILD_TYPE=Release -DASTRAL_VENDOR_SDL3=ON
cmake --build . --config Release

# Install to staging directory
cmake --install . --prefix ./package

# Create archive
tar -czf AstralEngine-v0.1.0-alpha.tar.gz package/
# zip -r AstralEngine-v0.1.0-alpha.zip package/  # Windows
```

### Windows Installer

```bash
# Enable CPack
cmake .. -DASTRAL_BUILD_INSTALLER=ON
cmake --build . --config Release
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
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Configure
      run: |
        cmake -B build \
          -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
          -DASTRAL_VENDOR_SDL3=ON
    
    - name: Build
      run: cmake --build build --config ${{ matrix.build_type }}
    
    - name: Test
      run: ctest --test-dir build -C ${{ matrix.build_type }}
```

---

## 📞 Support

If you encounter issues:

1. **Check the logs**: Look for error messages in the console output
2. **Verify dependencies**: Ensure all required libraries are installed
3. **Check CMake cache**: Delete `build/` directory and reconfigure
4. **Review integration guide**: See `SDL3_INTEGRATION_GUIDE.md` for detailed information
5. **Create an issue**: Include full error output and system information

---

## ✅ Success Indicators

Your SDL3 integration is working correctly if:

- ✅ CMake configuration completes without errors
- ✅ Build completes successfully
- ✅ Engine executable starts without crashing
- ✅ Window appears with correct title and size
- ✅ Keyboard and mouse input is detected
- ✅ Window can be resized and closed properly
- ✅ No SDL3-related error messages in logs

Congratulations! Your Astral Engine is now running with professional SDL3 integration.