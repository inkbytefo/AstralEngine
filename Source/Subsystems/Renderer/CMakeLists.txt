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
)

# Add dependencies specific to Renderer if any (Vulkan is already linked globally)
# target_link_libraries(AstralEngine PRIVATE ...)
