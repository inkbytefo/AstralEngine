#include "Engine.h"
#include "../Events/EventManager.h"
#include "../Subsystems/Platform/PlatformSubsystem.h"
#include "Logger.h"
#include <chrono>
#include <thread>

namespace AstralEngine {

Engine::Engine() { Logger::Info("Engine", "Engine instance created"); }

Engine::~Engine() {
  if (m_initialized) {
    Shutdown();
  }
  Logger::Info("Engine", "Engine instance destroyed");
}

void Engine::Run(IApplication *application) {
  if (m_isRunning) {
    Logger::Warning("Engine", "Engine is already running!");
    return;
  }

  m_application = application;
  if (!m_application) {
    Logger::Critical("Engine", "Application instance is null!");
    return;
  }

  Logger::Info("Engine", "Starting engine...");
  try {
    Initialize();
  } catch (const std::exception &e) {
    Logger::Critical("Engine", "Engine initialization failed: {}", e.what());
    return;
  }

  Logger::Info("Engine", "Starting application...");
  try {
    m_application->OnStart(this);
  } catch (const std::exception &e) {
    Logger::Error("Engine", "Application OnStart failed: {}", e.what());
    Shutdown();
    return;
  }

  m_isRunning = true;

  auto lastFrameTime = std::chrono::high_resolution_clock::now();

  // Cache subsystem lists for performance
  const auto &preUpdateSystems = m_subsystemsByStage[UpdateStage::PreUpdate];
  const auto &updateSystems = m_subsystemsByStage[UpdateStage::Update];
  const auto &postUpdateSystems = m_subsystemsByStage[UpdateStage::PostUpdate];
  const auto &uiSystems = m_subsystemsByStage[UpdateStage::UI];
  const auto &renderSystems = m_subsystemsByStage[UpdateStage::Render];

  while (m_isRunning) {
    try {
      auto currentTime = std::chrono::high_resolution_clock::now();
      auto deltaTime =
          std::chrono::duration<float>(currentTime - lastFrameTime).count();
      lastFrameTime = currentTime;

      // 1. Update engine-level systems
      Update();

      // 2. PreUpdate aşaması (Input, Platform Events)
      for (auto *subsystem : preUpdateSystems) {
        try {
          subsystem->OnUpdate(deltaTime);
        } catch (const std::exception &e) {
          Logger::Error("Engine", "PreUpdate failed for subsystem {}: {}",
                        subsystem->GetName(), e.what());
        }
      }

      // 3. Event processing
      EventManager::GetInstance().ProcessEvents();

      // 4. Main Update aşaması (Game Logic, ECS Systems)
      for (auto *subsystem : updateSystems) {
        try {
          subsystem->OnUpdate(deltaTime);
        } catch (const std::exception &e) {
          Logger::Error("Engine", "Update failed for subsystem {}: {}",
                        subsystem->GetName(), e.what());
        }
      }

      // 5. Application Logic Update
      try {
        m_application->OnUpdate(deltaTime);
      } catch (const std::exception &e) {
        Logger::Error("Engine", "Application update failed: {}", e.what());
      }

      // 6. PostUpdate aşaması (Physics, etc.)
      for (auto *subsystem : postUpdateSystems) {
        try {
          subsystem->OnUpdate(deltaTime);
        } catch (const std::exception &e) {
          Logger::Error("Engine", "PostUpdate failed for subsystem {}: {}",
                        subsystem->GetName(), e.what());
        }
      }

      // 7. UI aşaması (ImGui updates, command list generation)
      for (auto *subsystem : uiSystems) {
        try {
          subsystem->OnUpdate(deltaTime);
        } catch (const std::exception &e) {
          Logger::Error("Engine", "UI update failed for subsystem {}: {}",
                        subsystem->GetName(), e.what());
        }
      }

      // 8. Render aşaması
      for (auto *subsystem : renderSystems) {
        try {
          subsystem->OnUpdate(deltaTime);
        } catch (const std::exception &e) {
          Logger::Error("Engine", "Render failed for subsystem {}: {}",
                        subsystem->GetName(), e.what());
        }
      }

      // 9. Shutdown check
      auto *platformSubsystem = GetSubsystem<PlatformSubsystem>();
      if (platformSubsystem && platformSubsystem->GetWindow()->ShouldClose()) {
        RequestShutdown();
      }

    } catch (const std::exception &e) {
      Logger::Error("Engine", "Frame update failed: {}", e.what());
    }
  }

  Logger::Info("Engine", "Shutting down application...");
  try {
    m_application->OnShutdown();
  } catch (const std::exception &e) {
    Logger::Error("Engine", "Application OnShutdown failed: {}", e.what());
  }

  Shutdown();
  Logger::Info("Engine", "Engine shutdown complete");
}

void Engine::Initialize() {
  if (m_initialized) {
    Logger::Warning("Engine", "Engine already initialized!");
    return;
  }

  Logger::Info("Engine", "Initializing engine and subsystems...");

  // CRITICAL: Initialize subsystems in the exact order they were registered.
  // This respects dependency order (e.g. Render depends on Platform).
  for (auto &subsystem : m_subsystemsOwned) {
    try {
      Logger::Info("Engine", "Initializing subsystem: {}",
                   subsystem->GetName());
      subsystem->OnInitialize(this);
      Logger::Info("Engine", "Successfully initialized subsystem: {}",
                   subsystem->GetName());
    } catch (const std::exception &e) {
      Logger::Error("Engine", "Failed to initialize subsystem {}: {}",
                    subsystem->GetName(), e.what());
      throw std::runtime_error(
          std::string("Subsystem initialization failed: ") +
          subsystem->GetName() + " - " + e.what());
    }
  }

  m_initialized = true;
  Logger::Info("Engine", "Engine initialization complete");
}

void Engine::Shutdown() {
  if (!m_initialized) {
    return;
  }

  Logger::Info("Engine", "Shutting down engine and subsystems...");

  // CRITICAL: Shutdown systems in reverse registration order (LIFO).
  // This ensures dependencies are cleared correctly.
  for (auto it = m_subsystemsOwned.rbegin(); it != m_subsystemsOwned.rend();
       ++it) {
    try {
      Logger::Info("Engine", "Shutting down subsystem: {}", (*it)->GetName());
      (*it)->OnShutdown();
      Logger::Info("Engine", "Successfully shutdown subsystem: {}",
                   (*it)->GetName());
    } catch (const std::exception &e) {
      Logger::Error("Engine", "Failed to shutdown subsystem {}: {}",
                    (*it)->GetName(), e.what());
    }
  }

  // Clear ownership - this will call destructors in reverse order as well
  m_subsystemsOwned.clear();
  m_subsystemsByStage.clear();
  m_subsystemMap.clear();

  m_initialized = false;
  m_isRunning = false;

  Logger::Info("Engine", "Engine shutdown complete");
}

void Engine::Update() {
  // Engine levels tasks...
}

void Engine::RequestShutdown() {
  Logger::Info("Engine", "Shutdown requested");
  m_isRunning = false;
}

} // namespace AstralEngine
