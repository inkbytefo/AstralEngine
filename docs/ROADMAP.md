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

## Phase 2: Content Pipeline & Basic Rendering (🚧 Current Focus)
*Objective: Move from hardcoded geometry to a data-driven rendering pipeline capable of loading and displaying 3D assets.*

### 1. Mesh Loading & Rendering (Priority: High)
- [ ] **Geometry Abstraction:** Create `Mesh` class in Renderer that holds `VertexBuffer` and `IndexBuffer`.
- [ ] **Asset Integration:** Connect `AssetSubsystem` (Assimp) with RHI to upload loaded models to GPU.
- [ ] **Vertex Layout:** Abstract vertex input descriptions (Position, Normal, UV).

### 2. Camera System (Priority: High)
- [ ] **Camera Component:** ECS component for Camera properties (FOV, Near/Far, Aspect).
- [ ] **Camera Controller:** Free-look / Orbit camera script.
- [ ] **View/Projection Matrices:** Feed camera data to the Render System automatically.

### 3. Material System (Priority: Medium)
- [ ] **Material Asset:** Define a `Material` class that binds a Shader and its parameters (Textures, Colors).
- [ ] **Texture Support:** Integrate `stb_image` loading with RHI Texture creation.
- [ ] **Shader Reflection (Optional):** Automatic uniform layout detection (SPIR-V Reflect).

### 4. Basic Lighting (Priority: Medium)
- [ ] **Phong/Blinn-Phong:** Simple lighting model implementation.
- [ ] **Light Components:** Directional, Point, and Spot lights in ECS.

---

## Phase 3: Scene Management & ECS
*Objective: Transition to a fully Entity-Component-System (ECS) driven architecture.*

- [ ] **Transform Hierarchy:** Implement parent-child relationships in `TransformComponent`.
- [ ] **Scene Graph:** System to traverse and render the scene hierarchy.
- [ ] **Serialization:** Save and Load scenes to/from disk (JSON/YAML).
- [ ] **Selection System:** Raycasting to select entities in the 3D world.

---

## Phase 4: Editor Development
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
