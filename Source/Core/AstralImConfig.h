#pragma once

// Astral Engine's custom configuration for Dear ImGui
// Enhanced configuration based on IMGUI_INTEGRATION_GUIDE.md requirements
// inkbytefo

// Memory allocator support
#define IMGUI_USE_WCHAR32                    // Unicode desteği
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS     // Deprecated API'leri devre dışı bırak
#define IMGUI_DISABLE_OBSOLETE_KEYIO         // Eski input sistem

// Performance optimizations
#define IMGUI_DISABLE_DEMO_WINDOWS           // Release'te demo pencereyi kaldır
#define IMGUI_DISABLE_DEBUG_TOOLS            // Release'te debug araçları kaldır (şartlı)

// Integration features
#define IMGUI_DEFINE_MATH_OPERATORS          // Math operatörleri etkinleştir
// #define IMGUI_ENABLE_FREETYPE                // FreeType font rendering - Disabled until FreeType dependency is added

// Platform specific configurations
#ifdef ASTRAL_DEBUG
    #define IMGUI_DEBUG_PARANOID             // Extra debug checks
    #define IM_ASSERT(expr) assert(expr)     // Custom assert
#else
    #undef IMGUI_DISABLE_DEMO_WINDOWS        // Demo'yu sadece debug'da göster
    #undef IMGUI_DISABLE_DEBUG_TOOLS
#endif

// Astral Engine math integration
#include "Math/Vector2.h"
#include "Math/Vector4.h"

// Custom math types integration
#define IM_VEC2_CLASS_EXTRA                                         \
    constexpr ImVec2(const AstralEngine::Vector2& v) : x(v.x), y(v.y) {} \
    constexpr operator AstralEngine::Vector2() const { return AstralEngine::Vector2(x, y); }

#define IM_VEC4_CLASS_EXTRA                                             \
    constexpr ImVec4(const AstralEngine::Vector4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {} \
    constexpr operator AstralEngine::Vector4() const { return AstralEngine::Vector4(x, y, z, w); }

// Custom allocator support (forward declarations)
namespace AstralEngine {
    void* ImGuiAlloc(size_t size, void* user_data = nullptr);
    void ImGuiFree(void* ptr, void* user_data = nullptr);
}

// Disable warnings for undefined functions (custom allocator)
#define IMGUI_USER_CONFIG_NO_WARN_UNDEFINED_FUNCTIONS
