
# Astral Engine - Core Systems API Reference

This document provides the complete API reference for the core systems of the Astral Engine project.

## Engine Class

### Engine Class Overview
The `Engine` class serves as the main orchestrator for all subsystems. It manages the lifecycle of the entire engine and coordinates all subsystems.

#### Constructor and Initialization
```cpp
Engine();
```

#### Registering Subsystems
```cpp
template<typename T, typename... Args>
void RegisterSubsystem(Args&&... args);
```

#### Running the Engine
```cpp
void Run();
```

#### Shutdown
```cpp
void Shutdown();
```

## Logger System

### Log Levels
- Debug
- Info
- Warning
- Error
- Critical

### Logging Methods
```cpp
Logger::Debug("Category", "Message");
Logger::Info("Category", "Message");
Logger::Warning("Category", "Message");
Logger::Error("Category", "Message");
Logger::Critical("Category", "Message");
```

## Memory Management System

### Memory Allocation
```cpp
MemoryManager::Allocate<T>(size_t count);
MemoryManager::Deallocate<T>(T* ptr);
```

### Memory Statistics
```cpp
MemoryManager::GetMemoryStats();
```

## Event System

### Event Subscription
```cpp
EventManager::Subscribe("event", callback);
```

### Event Publishing
```cpp
EventManager::Publish("event", data);
```

## Entity Component System

### Component Registration
```cpp
ECS::RegisterComponent<T>();
```

### Entity Creation
```cpp
EntityHandle entity = ECS::CreateEntity();
```

### Component Attachment
```cpp
ECS::AddComponent<T>(entity, component);
```

## Asset Management

### Asset Loading
```cpp
AssetManager::Load<Texture2D>("Assets/Textures/logo.png");
```

### Shader Compilation
```cpp
ShaderCompiler::Compile("Assets/Shaders/triangle.hlsl");
```

## Input System

### Input Handling
```cpp
Input::IsKeyPressed(KeyCode::Space);
```

### Mouse Input
```cpp
Input::GetMousePosition();
```

## File System

### File Operations
```cpp
FileSystem::ReadAllText("path/to/file.txt");
```

### Path Resolution
```cpp
FileSystem::GetBasePath();
```

## Build System Integration

### CMake Configuration
```bash
cmake .. -DASTRAL_BUILD_EXAMPLES=ON -DCMAKE_BUILD_TYPE=Release
```

### Shader Compilation
```bash
cmake --build . --target Shaders
```

## Cross-Platform Considerations

### Platform-Specific Notes
- Windows: Uses Win32 API for window management
- Linux: Uses X11/Wayland for window management
- macOS: Uses Cocoa for window management

### Memory Management
- Uses custom allocators for all allocations
- Automatic memory leak detection in debug builds
- Detailed memory statistics available via `MemoryManager::GetMemoryStats()`

### Event System
- Thread-safe event handling
- Automatic event cleanup
- Supports custom event types

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLSL support

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLSL support

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reloading of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case-insensitive path resolution on Windows

### Build System
- Automatic dependency tracking
- Parallel build support
- Automatic rebuild on source changes

### Asset Pipeline
- Automatic texture compression
- Hot-reacting of assets in development builds
- Automatic dependency tracking

### Shader System
- Automatic shader compilation
- Runtime shader reloading
- Cross-platform HLSL/GLS

### Input System
- Raw input support for all platforms
- Automatic input mapping
- Gamepad support via SDL3

### Logging System
- Colored console output
- Automatic log rotation
- Log level filtering

### Memory Management
- Automatic memory leak detection
- Detailed allocation statistics
- Custom allocators support

### File System
- Cross-platform path handling
- Automatic path normalization
- Case