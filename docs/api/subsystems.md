
# Astral Engine - Subsystem Architecture Documentation

## Overview
The Astral Engine uses a modular subsystem architecture where each subsystem is responsible for a specific domain of functionality. This document outlines the architecture and responsibilities of each subsystem.

## Core Subsystems

### 1. Engine Core (`Engine` class)
- **Responsibility**: Central orchestrator for all subsystems
- **Key Features**:
  - Subsystem registration and lifecycle management
  - Event bus for subsystem communication
  - Cross-platform abstraction layer

### 2. Platform Subsystem (`PlatformSubsystem`)
- **Responsibility**: Window creation and input handling
- **Key Features**:
  - Cross-platform window management
  - Input device abstraction
  - Clipboard and monitor management

### 3. Render Subsystem (`RenderSubsystem`)
- **Responsibility**: 3D rendering pipeline management
- **Key Features**:
  - Vulkan rendering backend
  - Shader management
  - Framebuffer management

### 4. Asset Subsystem (`AssetSubsystem`)
- **Responsibility**: Asset loading and management
- **Key Features**:
  - Texture loading
  - Model loading
  - Hot-reloading support

### 5. ECS Subsystem (`ECSSubsystem`)
- **Responsibility**: Entity-Component-System implementation
- **Key Features**:
  - Entity management
  - Component storage
  - System execution

### 6. Scripting Subsystem (`ScriptingSubsystem`)
- **Responsibility**: Scripting language integration
- **Key Features**:
  - Lua integration
  - Hot-reloading support

### 7. Physics Subsystem (`PhysicsSubsystem`)
- **Responsibility**: Physics simulation
- **Key Features**:
  - Collision detection
  - Rigid body dynamics

### 8. Audio Subsystem (`AudioSubsystem`)
- **Responsibility**: Audio playback and management
- **Key Features**:
  - 3D spatial audio
  - Audio format support

### 9. Network Subsystem (`NetworkSubsystem`)
- **Responsibility**: Multiplayer networking
- **Key Features**:
  - Client-server architecture
  - Network serialization

### 10. Editor Subsystem (`EditorSubsystem`)
- **Responsibility**: In-editor functionality
- **Key Features**:
  - Scene editing
  - Asset management

## Subsystem Communication

### Event System
- **Responsibility**: Subsystem communication
- **Key Features**:
  - Event subscription
  - Event publishing

### Configuration System
- **Responsibility**: Settings management
- **Key Features**:
  - Cross-platform settings
  - Hot-reloading

### Logging System
- **Responsibility**: Logging and diagnostics
- **Key Features**:
  - Colored console output
  - Log rotation

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support
- Windows-specific path handling

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reloading of assets**

## Example Usage

### Basic Engine Initialization
```cpp
#include <AstralEngine/Engine.h>
#include <AstralEngine/PlatformSubsystem.h>
#include <AstralEngine/RenderSubsystem.h>

int main() {
    Engine engine;
    engine.RegisterSubsystem<PlatformSubsystem>();
    engine.RegisterSubsystem<RenderSubsystem>();
    engine.Run();
    return 0;
}
```

### Subsystem Communication Example
```cpp
// Subscribe to events
EventManager::Subscribe("window-resize", [](const WindowResizeEvent& event) {
    // Handle window resize
});

// Publish events
EventManager::Publish("engine-startup", {});
```

### Asset Pipeline Example
```cpp
// Load texture with automatic format detection
AssetManager::Load<Texture2D>("Assets/Textures/logo.png");

// Compile shader with automatic recompilation
ShaderCompiler::Compile("Shaders/triangle.hlsl");
```

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reloading of assets**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reloading of assets**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reloading of assets**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic shader compilation**
- **Runtime shader reloading**

### Asset Pipeline
- **Automatic texture compression**
- **Hot-reacting of assets in development builds**

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayland support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- **Automatic dependency tracking**
- **Parallel build support**
- **Cross-platform build configuration**

### Shader Compilation
- **Automatic