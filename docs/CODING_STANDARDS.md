# Coding Standards

To maintain a professional and sustainable codebase, we adhere to the following strict standards.

## General Principles
- **Production-Grade:** No "template only" or pseudo-code. All code must compile and run.
- **Integrity:** Explicitly handle edge cases. No "happy path" programming.
- **Verify:** Always verify changes with tests or runnable examples.

## C++ Standards
- **Standard:** C++20.
- **Style:** Google C++ Style Guide (adapted).
    - PascalCase for Types and Functions (`MyClass`, `DoSomething`).
    - camelCase for variables (`myVariable`).
    - `m_` prefix for private members (`m_window`).
- **Safety:**
    - Use `std::shared_ptr` and `std::unique_ptr` for ownership.
    - Avoid raw pointers unless observing (non-owning).
    - Use `std::span` or `std::vector` instead of C-arrays.
    - `nodiscard` on functions where return value matters.

## Renderer Specifics
- **Vulkan:**
    - Always check `VkResult`. Throw exceptions on critical failures.
    - Use VMA for all allocations.
    - Use Dynamic Rendering (no RenderPasses).
    - Synchronization is the caller's responsibility (mostly handled by RHI).

## Git Workflow
- **Commits:** Atomic, descriptive messages.
- **Branches:** Feature-based development.

## Testing
- Every major feature should have a verification step (e.g., `RenderTest`).
- Run `RenderTest` after touching the RHI to ensure no regressions.
