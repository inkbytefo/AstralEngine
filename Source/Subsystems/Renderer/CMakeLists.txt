# Source/Subsystems/Renderer/CMakeLists.txt

# Add Renderer sources to the main engine target
target_sources(AstralEngine PRIVATE
    # RHI - Vulkan Implementation
    "RHI/Vulkan/VulkanDevice.cpp"
    "RHI/Vulkan/VulkanResources.cpp"
    "RHI/Vulkan/VulkanCommandList.cpp"
    "RHI/Vulkan/VulkanMemoryAllocator.cpp"
    
    # Core (To be implemented)
    "Core/RenderSubsystem.cpp" 
    "Core/Mesh.cpp"
    "Core/Mesh.h"
    "Core/Texture.cpp"
    "Core/Texture.h"
    "Core/Camera.cpp"
    "Core/Camera.h"
    "Core/Material.cpp"
    "Core/Material.h"
)

# Add dependencies specific to Renderer if any (Vulkan is already linked globally)
# target_link_libraries(AstralEngine PRIVATE ...)
