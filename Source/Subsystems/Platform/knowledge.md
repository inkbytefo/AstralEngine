# Platform Subsystem - SDL3 Integration Knowledge

## Overview

The Platform Subsystem provides a cross-platform abstraction layer using SDL3 (Simple DirectMedia Layer 3) for window management, input handling, and system events. This implementation follows enterprise-grade practices with comprehensive error handling, professional documentation, and MSVC compatibility.

## Architecture

### Core Components

1. **PlatformSubsystem**: Main orchestrator for platform-dependent operations
2. **Window**: SDL3-based window abstraction with full lifecycle management
3. **InputManager**: Input handling and state management for keyboard/mouse
4. **EventManager Integration**: Bridges SDL3 events to the engine's event system

### SDL3 Integration Strategy

- **Conditional Compilation**: All SDL3 code is wrapped with `#ifdef ASTRAL_USE_SDL3`
- **Professional Error Handling**: Comprehensive validation and logging
- **Resource Management**: RAII principles with smart pointers
- **Thread Safety**: Atomic operations for shared state
- **Performance Optimization**: Cached data and minimal allocations

## Window Management

### Key Features

- **Modern SDL3 API**: Uses latest SDL3 features and best practices
- **Flexible Window Creation**: Supports various window modes and configurations
- **Event Processing**: Complete SDL3 event loop integration
- **Platform Handles**: Direct access to native window handles for graphics APIs
- **State Caching**: Optimized property access with cached values

### SDL3 Window Creation Process

```cpp
// 1. Initialize SDL3 subsystems
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)

// 2. Create window with flags
SDL_CreateWindow(title, width, height, flags)

// 3. Configure window properties
SDL_SetWindowTitle(), SDL_SetWindowSize(), etc.

// 4. Event loop integration
SDL_PollEvent() → HandleWindowEvent()
```

### Supported Window Features

- Resizable windows with aspect ratio management
- Fullscreen and windowed modes
- VSync control and buffer swapping
- Window focus and visibility states
- Platform-specific native handles (HWND on Windows)
- Window minimization/maximization
- File drag & drop support

## Input System

### Design Philosophy

- **Frame-Based State**: Current and previous frame comparison
- **Event-Driven Updates**: Real-time input processing
- **Type Safety**: Strongly typed key codes and mouse buttons
- **Performance**: Bitset-based state storage for O(1) queries

### Key Mapping Strategy

```cpp
// SDL3 → Engine mapping functions
KeyCode SDLKeyToAstralKey(SDL_Keycode sdlKey);
MouseButton SDLButtonToAstralButton(uint8_t sdlButton);
```

### Input State Management

- **Current Frame State**: `m_keyboardState`, `m_mouseState`
- **Previous Frame State**: `m_keyboardStatePrevious`, `m_mouseStatePrevious`
- **Delta Calculations**: Mouse movement and wheel deltas
- **Just Pressed/Released Detection**: Frame-to-frame comparisons

## Event System Integration

### SDL3 → Engine Event Mapping

| SDL3 Event | Engine Event | Description |
|------------|--------------|-------------|
| `SDL_EVENT_QUIT` | `WindowCloseEvent` | Application quit requested |
| `SDL_EVENT_WINDOW_RESIZED` | `WindowResizeEvent` | Window size changed |
| `SDL_EVENT_KEY_DOWN` | `KeyPressedEvent` | Key pressed (with repeat detection) |
| `SDL_EVENT_KEY_UP` | `KeyReleasedEvent` | Key released |
| `SDL_EVENT_MOUSE_BUTTON_DOWN` | `MouseButtonPressedEvent` | Mouse button pressed |
| `SDL_EVENT_MOUSE_BUTTON_UP` | `MouseButtonReleasedEvent` | Mouse button released |
| `SDL_EVENT_MOUSE_MOTION` | `MouseMovedEvent` | Mouse cursor moved |

### Event Processing Flow

1. **SDL3 Event Poll**: `SDL_PollEvent()` in main loop
2. **Event Filtering**: Window ID validation and event type checking
3. **Data Conversion**: SDL3 data → Engine data structures
4. **Event Publishing**: `EventManager::PublishEvent<T>()`
5. **Subscriber Notification**: Event system distributes to subscribers
6. **InputManager Integration**: PlatformSubsystem subscribes to events and forwards to InputManager

### InputManager Event Integration

The PlatformSubsystem acts as a bridge between the Window's event publishing and the InputManager's state tracking:

```cpp
// In PlatformSubsystem::OnInitialize()
auto keyPressedId = eventManager.Subscribe<KeyPressedEvent>([this](Event& event) {
    auto& e = static_cast<KeyPressedEvent&>(event);
    m_inputManager->HandleSDLKeyEvent(e.GetKeyCode(), true);
    return false; // Don't consume the event
});
```

This design allows:
- **Decoupled Architecture**: InputManager doesn't directly depend on SDL3
- **Event Transparency**: Other systems can also subscribe to the same events
- **Clean Lifecycle**: Subscriptions are properly managed and cleaned up
- **Type Safety**: Compile-time event type checking

## Build Configuration

### CMake Integration

- **Multi-Strategy Detection**: Config mode, pkg-config, FetchContent
- **Version Control**: Targets specific SDL3 version (3.2.22)
- **Build Options**: Optimized SDL3 compilation flags
- **Dependency Management**: Automatic DLL copying on Windows

### Compiler Definitions

- `ASTRAL_USE_SDL3`: Enables SDL3 code paths
- Platform-specific definitions for Windows, Linux, macOS
- Build type definitions (Debug, Release, etc.)

## Error Handling

### Professional Error Management

```cpp
// SDL3 error checking pattern
if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    Logger::Error("Window", "SDL initialization failed: {}", SDL_GetError());
    return false;
}
```

### Recovery Strategies

- **Graceful Degradation**: Fallback to placeholder implementations
- **Resource Cleanup**: Automatic cleanup on failure
- **Detailed Logging**: Comprehensive error reporting
- **State Validation**: Defensive programming practices

## Performance Considerations

### Optimization Techniques

- **Cached Window Properties**: Avoid repeated SDL3 queries
- **Atomic State Variables**: Lock-free thread safety
- **Minimal Allocations**: Reuse of data structures
- **Efficient Event Processing**: Fast path for common events

### Memory Management

- **RAII Principles**: Smart pointers for automatic cleanup
- **Move Semantics**: Efficient resource transfer
- **Stack Allocation**: Prefer stack over heap when possible
- **Resource Pooling**: Future consideration for high-frequency objects

## Platform-Specific Implementation

### Windows (MSVC)

- **Native Handle Access**: `GetHWND()` for DirectX/OpenGL integration
- **DLL Management**: Automatic SDL3.dll copying
- **Windows-Specific Events**: Focus, activation, etc.
- **High-DPI Support**: Pixel vs. screen coordinate handling

### Linux

- **X11/Wayland Support**: Automatic backend detection
- **Package Manager Integration**: pkg-config fallback
- **Desktop Environment Integration**: Proper window management

### macOS

- **Cocoa Integration**: Native macOS window handling
- **Retina Display Support**: High-resolution display handling
- **Metal Integration**: Future graphics API support

## Future Enhancements

### Planned Features

1. **Graphics Context Management**: OpenGL/Vulkan context creation
2. **Audio Integration**: SDL3 audio subsystem support
3. **Gamepad Support**: Controller input handling
4. **Haptic Feedback**: Force feedback and vibration
5. **Multi-Monitor Support**: Enhanced display management
6. **Custom Cursors**: Cursor customization and hiding

### Performance Improvements

1. **Event Batching**: Batch multiple events per frame
2. **Memory Pool**: Object pooling for events
3. **SIMD Optimizations**: Vectorized operations where applicable
4. **Profiling Integration**: Performance metrics and timing

## Best Practices

### Code Quality

- **Const Correctness**: Proper const usage throughout
- **Exception Safety**: RAII and exception-safe code
- **Documentation**: Comprehensive inline documentation
- **Testing**: Unit tests for core functionality

### Maintenance

- **Version Management**: Clear SDL3 version dependencies
- **Backward Compatibility**: Graceful handling of older SDL3 versions
- **Code Reviews**: Peer review of platform-specific code
- **Continuous Integration**: Automated testing across platforms

## Troubleshooting

### Common Issues

1. **SDL3 Not Found**: Check installation paths and CMake configuration
2. **DLL Missing**: Ensure SDL3.dll is in executable directory
3. **Event Not Firing**: Verify event loop and subscription setup
4. **Performance Issues**: Check for excessive logging or polling

### Debug Tools

- **Logger Integration**: Comprehensive logging at all levels
- **Event Tracing**: Detailed event flow logging
- **State Inspection**: Runtime state debugging
- **Performance Profiling**: Timing and memory usage analysis

This knowledge base serves as the authoritative guide for SDL3 integration within the Astral Engine Platform Subsystem.