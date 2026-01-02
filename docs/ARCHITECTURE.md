# System Architecture

Astral Engine follows a strict **Subsystem-based Architecture**, managed by a central `Engine` class. This design ensures separation of concerns, deterministic initialization/shutdown orders, and easy extensibility.

## Core Components

### 1. Engine (`Core/Engine`)
The `Engine` class is the root object. It is responsible for:
- Managing the lifecycle of the application.
- Initializing and shutting down subsystems in a specific order.
- The main game loop (Update, Render).

### 2. Subsystems (`Core/ISubsystem`)
All major functionalities are encapsulated as subsystems inheriting from `ISubsystem`.
- **PlatformSubsystem:** Handles OS-specific tasks (Windowing, Input) via SDL3. Features a **Robust Windowing** logic that falls back to standard windows if Vulkan-tagged window creation fails (e.g., in RDP or old driver environments).
- **RenderSubsystem:** Manages the RHI device and rendering operations. Includes manual Win32 surface creation logic to support the platform fallback.
- **UISubsystem:** Integration of ImGui for debug UI and tools.
- **AssetSubsystem:** Manages loading and caching of assets (Models, Textures, Shaders).

### 3. Application (`Core/IApplication`)
The user application inherits from `IApplication`. It serves as the bridge between the Engine and game logic.
- `OnInitialize()`: Register subsystems, load initial assets.
- `OnUpdate(dt)`: Game logic update.
- `OnRender()`: Dispatch render commands.

## Data Flow

1.  **Initialization:**
    - `Engine` starts.
    - Registers Core Subsystems (Platform, Asset, Render, UI).
    - Calls `OnInitialize` on all subsystems.
    - Calls `App->OnInitialize`.

2.  **Frame Loop:**
    - **Poll Events:** PlatformSubsystem pumps OS events (Input, Resize).
    - **App Update:** `App->OnUpdate(dt)` is called.
    - **Subsystem Update:** All subsystems receive `OnUpdate`.
    - **Render:** RenderSubsystem executes the frame graph (BeginFrame -> Render -> Present).

3.  **Shutdown:**
    - Reverse order of initialization.
    - RAII ensures resources are freed correctly.

## Event System (`Events/`)
A decoupled event system allows communication between subsystems without direct dependencies.
- **Types:** `ApplicationEvent`, `KeyEvent`, `MouseEvent`, `WindowEvent`.
- **Dispatch:** Events are propagated from the Platform layer down to the Engine and Application.
