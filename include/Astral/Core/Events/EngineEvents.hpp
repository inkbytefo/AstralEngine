#pragma once

#include "Astral/Core/EntityHandle.hpp"
#include <string>
#include <cstdint>

namespace Astral {

class Scene;

/// Sahne icerisinde bir nesne secildiginde tetiklenir (Editor, Inspector, Gizmo, Raymarch Picking).
struct EntitySelectedEvent {
    EntityHandle entity = NullEntityHandle;
    Scene* scene = nullptr;
};

/// Yeni bir sahne yuklendiginde veya olusturuldugunda tetiklenir.
struct SceneLoadedEvent {
    std::string sceneName;
    Scene* scene = nullptr;
};

/// Sahne diske ikili (binary) formatta kaydedildiginde tetiklenir.
struct SceneSavedEvent {
    std::string scenePath;
};

/// Pencere veya swapchain boyutu degistiginde tetiklenir.
struct WindowResizeEvent {
    uint32_t width = 0;
    uint32_t height = 0;
};

/// Editor ile Play/Runtime modu arasinda gecis yapildiginda tetiklenir.
struct PlayModeChangedEvent {
    bool isPlaying = false;
};

} // namespace Astral
