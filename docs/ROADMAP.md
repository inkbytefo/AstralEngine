# Development Roadmap

This document outlines the strategic development plan for **Astral Engine**. It serves as a guide for the "Project Lead" (Agent) and the User to track progress and prioritize tasks.

## Phase 1: Foundation (✅ Completed)
*Objective: Establish the core architecture and a modern rendering backend.*

- [x] **Core Architecture:** Subsystem-based engine lifecycle (Initialize, Update, Shutdown).
- [x] **Platform Layer:** SDL3 integration for Windowing and Input.
- [x] **RHI (Render Hardware Interface):**
    - [x] Vulkan 1.3 Backend.
    - [x] Dynamic Rendering (No RenderPass/Framebuffer).
    - [x] VMA (Vulkan Memory Allocator) integration.
    - [x] Staging Buffers & Per-Frame UBOs.
    - [x] Depth Buffering.
- [x] **UI System:** ImGui integration with Dynamic Rendering.
- [x] **Basic Asset System:** Importer structure (Model, Texture, Shader).

---

## Phase 2: Content Pipeline & Basic Rendering (✅ Completed)
*Objective: Move from hardcoded geometry to a data-driven rendering pipeline capable of loading and displaying 3D assets.*

### 1. Mesh Loading & Rendering (✅ Completed)
- [x] **Geometry Abstraction:** Create `Mesh` class in Renderer that holds `VertexBuffer` and `IndexBuffer`.
- [x] **Asset Integration:** Connect `AssetSubsystem` (Assimp) with RHI to upload loaded models to GPU.
- [x] **Vertex Layout:** Abstract vertex input descriptions (Position, Normal, UV).

### 2. Camera System (✅ Completed)
- [x] **Camera Component:** Camera class with view/projection calculation.
- [x] **Camera Controller:** Free-look / Orbit camera input handling.
- [x] **View/Projection Matrices:** Feed camera data to the Render System automatically.

### 3. Material System (✅ Completed)
- [x] **Material Asset:** Define a `Material` class that binds a Shader and its parameters (Textures, Colors).
- [x] **Texture Support:** Integrate `stb_image` loading with RHI Texture creation.
- [x] **Shader Reflection (Optional):** Automatic uniform layout detection (SPIR-V Reflect).

### 4. Basic Lighting (✅ Completed)
- [x] **Phong/Blinn-Phong:** Simple lighting model implementation.
- [x] **Light Components:** Directional, Point, and Spot lights in ECS.

---

## Phase 3: Scene Management & ECS (✅ Completed)
*Objective: Transition to a fully Entity-Component-System (ECS) driven architecture.*

- [x] **Transform Hierarchy:** Implement parent-child relationships in `TransformComponent`.
- [x] **Scene Graph:** System to traverse and render the scene hierarchy.
- [x] **Serialization:** Save and Load scenes to/from disk (JSON/YAML).
- [x] **Selection System:** Raycasting to select entities in the 3D world.

---

## Phase 4: Editor Development (🚧 Current Focus)
*Objective: Create a usable WYSIWYG editor environment.*

- [ ] **Editor Viewport:** Render the game scene into an ImGui window (Texture ID).
- [ ] **Entity Inspector:** View and modify ECS component values at runtime.
- [ ] **Scene Hierarchy Panel:** Tree view of entities.
- [ ] **Asset Browser:** File explorer for importing and managing assets.
- [ ] **Gizmos:** 3D manipulation tools (Translate, Rotate, Scale).

---

## Phase 5: Advanced Rendering
*Objective: Achieve professional-grade visual fidelity.*

- [ ] **PBR (Physically Based Rendering):** Implement Metallic/Roughness workflow.
- [ ] **IBL (Image Based Lighting):** HDR environment maps.
- [ ] **Shadow Mapping:** CSM (Cascaded Shadow Maps) and Point Light shadows.
- [ ] **Post-Processing Stack:**
    - [ ] Tone Mapping (ACES).
    - [ ] Bloom.
    - [ ] Anti-Aliasing (FXAA/TAA).
- [ ] **Compute Shaders:** GPU-driven particle systems or culling.

---

## Future Research (Long Term)
- **Scripting:** C# (Mono) or Lua integration.
- **Physics:** Jolt Physics or PhysX integration.
- **Audio:** 3D Spatial Audio (fmod/OpenAL).
- **Multi-threading:** Job System for parallel command recording.
