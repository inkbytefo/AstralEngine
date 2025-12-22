# Astral Engine Code Review and Refactoring Plan

The goal is to analyze the Astral Engine codebase to identify and categorize code as active, dead, redundant, or duplicate. This will result in a comprehensive report and a prioritized cleaning plan.

## User Review Required

> [!IMPORTANT]
> This analysis is read-only. No changes will be made to the project files without explicit user approval of the final cleaning plan.

## Proposed Analysis Steps

### 1. Static Analysis for Dead Code
- **Method**: Use `grep_search` to find all function and class definitions, then cross-reference with usages.
- **Tools**: `grep_search`, `find_by_name`.
- **Target**: Unused functions, private members, and entire files that are not included in the build or referenced.

### 2. Redundancy and Complexity Analysis
- **Method**: Review "advanced" or "experimental" features noted in docs (like `GameUISubsystem.md`) and compare with current engine core functionality.
- **Target**: Features that are functionally complete but conceptually redundant in the current architecture.

### 3. Duplicate Code and Logic Detection
- **Method**:
    - **Exact Duplicates**: Search for identical file contents or large identical blocks using hash-based comparisons or string matching.
    - **Logic Duplicates**: Identify patterns in UI (ImGui), math operations, and resource management where similar logic is implemented multiple times.

### 4. Code Review and Bug Detection
- **Method**: Look for obvious code smells, potential memory leaks (manual memory management), and TODOs/FIXMEs.

## Verification Plan

### Automated Verification
- **Build Check**: Ensure the project still compiles (using existing `CMakeLists.txt` and `build.log` as reference).
- **Unit Tests**: Run tests in the `Tests` directory to ensure no regressions in current functionality.
    - Command: `cmake --build build --target Tests` (Verify exact target name from `Tests/CMakeLists.txt`).

### Manual Verification
- **Report Review**: Present the analysis table to the user for validation.
- **Sanity Check**: Manually verify selected "dead code" candidates by checking for indirect usages (e.g., via macros or reflection).
