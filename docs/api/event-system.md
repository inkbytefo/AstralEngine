
# Astral Engine Event System Documentation

## Overview
The event system provides a decoupled communication mechanism between subsystems and game objects.

## Event Flow
1. **Event Creation** → **Event Broadcasting** → **Handler Execution**

## Core Components

### Event Manager (EventManager)
- **Responsibility**: Central event bus for the entire engine
- **Key Features**:
  - Thread-safe event handling
  - Cross-subsystem communication
  - Automatic cleanup on shutdown

### Event Types
- **Engine Events**: System-level events
- **Input Events**: Keyboard, mouse, gamepad
- **Window Events**: Resize, focus, etc.
- **Custom Events**: Game-specific events

### Event Handlers
- **Global Handlers**: Engine-wide event handling
- **Subsystem Handlers**: Subsystem-specific event handling
- **Object Handlers**: Game object event handling

## Usage Examples

### Publishing Events
```cpp
// Publish a simple event
EventManager::Publish("game-start", {});

// Publish with data
EventManager::Publish("player-spawn", {{"position", "0,0,0"}});
```

### Subscribing to Events
```cpp
// Subscribe to all events
EventManager::Subscribe("game-start", []() {
    // Handle game start
});

// Subscribe to specific events
EventManager::Subscribe("player-spawn", [this](const EventData& data) {
    // Handle player spawn
});
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
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

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
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/Wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径处理

### macOS
- Cocoa API for窗口管理
- Metal/Vulkan支持

## Build System Integration

### CMake Configuration
- Automatic dependency tracking
- Parallel build support
- Cross-platform build configuration

### Shader Compilation
- Automatic shader compilation
- Runtime shader reloading

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds

## Cross-Platform Considerations

### Windows
- Win32 API for window management
- DirectX/Vulkan support

### Linux
- X11/wayful support
- POSIX路径