#pragma once

// ============================================================================
// AstralEngine Umbrella Header
// ----------------------------------------------------------------------------
// Oyun ve istemci projelerinin tek bir #include ile motorun temel API'sine
// (Application, ISubsystem, Registry, Components, Scene, Window, Temel Tipler)
// erismesini saglayan ana baslik dosyasidir.
// ============================================================================

// --- Core API ---
#include "Astral/Core/Application.hpp"
#include "Astral/Core/ISubsystem.hpp"
#include "Astral/Core/SystemManager.hpp"
#include "Astral/Core/Registry.hpp"
#include "Astral/Core/Components.hpp"
#include "Astral/Core/RenderExtractionSystem.hpp"
#include "Astral/Core/EntityHandle.hpp"
#include "Astral/Core/Window.hpp"
#include "Astral/Core/InputSystem.hpp"
#include "Astral/Core/BenchmarkLogger.hpp"

// --- Scene Management ---
#include "Astral/Scene/Scene.hpp"
#include "Astral/Scene/SceneManager.hpp"
#include "Astral/Scene/SceneSerializer.hpp"

// --- Project Management ---
#include "Astral/Project/Project.hpp"
#include "Astral/Project/ProjectSerializer.hpp"

// --- Math (GLM) ---
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
